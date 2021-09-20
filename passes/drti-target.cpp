// -*- mode:c++ -*-
//
// Module drti-target.cpp
//
// Copyright (c) 2019, 2020 Raoul M. Gough
//
// This file is part of DRTI.
//
// DRTI is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 only.
//
// DRTI is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// History
// =======
// 2020/02/03   rmg     File creation
//

#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/CSEConfigBase.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/ScheduleDAGInstrs.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/MC/MCContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"

#include <array>
#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <drti/configuration.hpp>

using namespace llvm;

namespace drti
{
    //! Handle the passing of drti::treenode* between caller and
    //! callee via a register. This is similar to the Swift calling
    //! convention's context parameter but without using a custom
    //! calling convention and the parameter is fully optional both at
    //! callsite and landing site.
    class X86DrtiTreenodePass : public MachineFunctionPass
    {
    public:
        static void maybeAddSelf(TargetPassConfig&);

        X86DrtiTreenodePass();

        bool runOnMachineFunction(MachineFunction &MF) override;

    private:
        void resolveCaller(MachineFunction &MF, MachineInstr&);
        void resolveSetCaller(MachineFunction &MF, MachineInstr&);
        MachineOperand& nextUse(MachineInstr& start, unsigned reg);
        MachineInstr& nextCall(MachineInstr& start);
        void insertInlineAsm(MachineFunction& MF, MachineInstr& sink);

    public:
        static char ID;
    };
}

char drti::X86DrtiTreenodePass::ID = 0;

static RegisterTargetExtension registrar(
    TargetPassConfig::MPEP_PreRegAlloc,
    &drti::X86DrtiTreenodePass::maybeAddSelf);

void drti::X86DrtiTreenodePass::maybeAddSelf(TargetPassConfig& tpc)
{
    if(tpc.getTM<TargetMachine>().getTargetTriple().str() ==
       "x86_64-unknown-linux-gnu")
    {
        tpc.addPass(new X86DrtiTreenodePass);
    }
}

drti::X86DrtiTreenodePass::X86DrtiTreenodePass() :
    MachineFunctionPass(ID)
{
}

bool drti::X86DrtiTreenodePass::runOnMachineFunction(MachineFunction &MF)
{
    DEBUG_WITH_TYPE("drti", llvm::dbgs() << "drti: runOnMachineFunction " << MF.getName() << "\n");

    SmallVector<MachineInstr *, 4> erasures;

    for(MachineBasicBlock& block: MF)
    {
        for(MachineInstr& inst: block)
        {
            if(inst.isCall() && inst.getNumOperands() > 0)
            {
                MachineOperand callee = inst.getOperand(0);

                if(callee.isGlobal())
                {
                    const GlobalValue* calleeGlobal = callee.getGlobal();

                    if(calleeGlobal && calleeGlobal->getName() == "_drti_set_caller")
                    {
                        // Replace uses of the call result
                        resolveSetCaller(MF, inst);
                        // Remove the call
                        erasures.push_back(&inst);
                    }
                    else if(calleeGlobal && calleeGlobal->getName() == "_drti_caller")
                    {
                        // Replace uses of the call result
                        resolveCaller(MF, inst);
                        // Remove the call
                        erasures.push_back(&inst);
                    }
                }
            }
        }
    }

    if(erasures.empty())
    {
        DEBUG_WITH_TYPE("drti", llvm::dbgs() << "drti: runOnMachineFunction " << MF.getName() << " done with no erasures\n");
        return false;
    }
    else
    {
        for(MachineInstr* target: erasures)
        {
            target->eraseFromParent();
            // This leaves the callframe setup and teardown in place,
            // but hopefully these get optimized away later
        }
        DEBUG_WITH_TYPE("drti", llvm::dbgs() << "drti: runOnMachineFunction " << MF.getName() << " erased " << erasures.size() << " calls\n");
        return true;
    }
}

MachineOperand& drti::X86DrtiTreenodePass::nextUse(
    MachineInstr& start, unsigned target)
{
    MachineBasicBlock& MBB(*start.getParent());

    for(auto iter = ++start.getIterator(); iter != MBB.end(); ++iter)
    {
        for(MachineOperand& op: iter->uses())
        {
            if(op.isReg() && op.getReg() == target)
            {
                return op;
            }
        }
    }

    report_fatal_error(
        "X86DrtiTreenodePass: RAX not found in block after _drti_caller");
}

void drti::X86DrtiTreenodePass::resolveCaller(MachineFunction& MF, MachineInstr& call)
{
    DEBUG_WITH_TYPE(
        "drti",
        llvm::dbgs() << "drti: runOnMachineFunction resolveCaller: ";
        call.print(llvm::dbgs()));

    const TargetSubtargetInfo& subTarget(MF.getSubtarget());
    const TargetLowering *TLI(subTarget.getTargetLowering());
    const TargetRegisterInfo *RI(subTarget.getRegisterInfo());

    if(!TLI || !RI)
    {
        report_fatal_error(
            "X86DrtiTreenodePass: unable to query target");
    }

    // For DRTI calls, R14 holds the treenode* on entry
    std::pair<unsigned, const TargetRegisterClass *> r14 =
        TLI->getRegForInlineAsmConstraint(
            RI, "{r14}", MVT::i64);

    // Physical register RAX gets the result of _drti_caller which
    // we need to subsitute
    std::pair<unsigned, const TargetRegisterClass *> rax =
        TLI->getRegForInlineAsmConstraint(
            RI, "{rax}", MVT::i64);

    if(!r14.first || !rax.first)
    {
        report_fatal_error(
            "X86DrtiTreenodePass: unable to find register by name");
    }

    MachineBasicBlock& entry(*MF.begin());

    if(entry.isLiveIn(r14.first))
    {
        report_fatal_error(
            "X86DrtiTreenodePass: call chain register already live");
    }

    // Find the first use of RAX after the bogus call (i.e. the use of
    // the _drti_caller() return value)
    MachineOperand& raxUse(nextUse(call, rax.first));

    unsigned virt = entry.addLiveIn(r14.first, r14.second);

    // Replace the use of RAX with the virtual register containing the
    // treenode from r14 on entry and kill the virtual reg. For this
    // to work we have to run before register allocation, of course.
    raxUse.setReg(virt);
    raxUse.setImplicit(false);
    raxUse.setIsKill(true);

    // TODO - probably we're supposed to kill the virtual register
    // copy of r14 in all the other successors of the entry block as
    // well
}

MachineInstr& drti::X86DrtiTreenodePass::nextCall(MachineInstr& start)
{
    MachineBasicBlock& MBB(*start.getParent());

    for(auto iter = ++start.getIterator(); iter != MBB.end(); ++iter)
    {
        if(iter->isCall() && iter->getNumOperands() > 0)
        {
            return *iter;
        }
    }
    report_fatal_error(
        "X86DrtiTreenodePass: No call found in block after _drti_set_caller");
}

void drti::X86DrtiTreenodePass::resolveSetCaller(
    MachineFunction& MF, MachineInstr& call)
{
    DEBUG_WITH_TYPE(
        "drti",
        llvm::dbgs() << "drti: runOnMachineFunction resolveSetCaller: ";
        call.print(llvm::dbgs()));

    const TargetSubtargetInfo& subTarget(MF.getSubtarget());
    const TargetInstrInfo* TII(subTarget.getInstrInfo());
    const TargetLowering *TLI(subTarget.getTargetLowering());
    const TargetRegisterInfo *RI(subTarget.getRegisterInfo());
    MachineBasicBlock& MBB(*call.getParent());

    if(!TLI || !RI)
    {
        report_fatal_error(
            "X86DrtiTreenodePass: unable to query target");
    }

    // Find the first call after the _drti_set_caller and assume this
    // is the one that is supposed to get the hidden treenode argument
    MachineInstr& sink(nextCall(call));

    // For DRTI calls, pass treenode* in R14
    std::pair<unsigned, const TargetRegisterClass *> r14 =
        TLI->getRegForInlineAsmConstraint(
            RI, "{r14}", MVT::i64);

    // The call to _drti_set_caller passes its argument in RDI
    // TODO - we should find the def of RDI in the call setup sequence
    // and change its target operand to be R14 instead of forcing the
    // value through RDI
    std::pair<unsigned, const TargetRegisterClass *> rdi =
        TLI->getRegForInlineAsmConstraint(
            RI, "{rdi}", MVT::i64);

    // Copy the RDI call argument into r14
    BuildMI(MBB, &call, call.getDebugLoc(), TII->get(TargetOpcode::COPY), r14.first)
        .addReg(rdi.first, RegState::Kill);

    // Mark R14 as killed at the call that implicitly passes the
    // treenode*
    sink.addRegisterKilled(r14.first, RI, true);

    insertInlineAsm(MF, sink);
}

void drti::X86DrtiTreenodePass::insertInlineAsm(
    MachineFunction& MF, MachineInstr& sink)
{
    // Insert some inline assembly before the "sink" call instruction
    // such that the return address is on a DRTI_RETALIGN boundary and
    // our magic value and static data are avilable from (retaddr() -
    // DRTI_RETALIGN). In order to make this work we emit labels
    // around the call instruction so the assembler can calculate the
    // padding. The drti-decorate pass should have made the call
    // "notail" so it can't be turned into a jmp (which would discard
    // the postCallSymbol).

    const TargetSubtargetInfo& subTarget(MF.getSubtarget());
    const TargetInstrInfo* TII(subTarget.getInstrInfo());

    const char preName[] = "_drti_pre_call";
    const char postName[] = "_drti_post_call";

    MF.getContext().setUseNamesOnTempLabels(true);
    MCSymbol* preCallSymbol = MF.getContext().createTempSymbol(preName, true);
    MCSymbol* postCallSymbol = MF.getContext().createTempSymbol(postName, true);

    MF.getContext().registerInlineAsmLabel(preCallSymbol);
    MF.getContext().registerInlineAsmLabel(postCallSymbol);

    // Do we need to put the inline asm before all the call frame
    // setup instructions or can it go right before the call?

    {
        MachineInstrBuilder MIB = BuildMI(
            *sink.getParent(), sink, sink.getDebugLoc(),
            TII->get(TargetOpcode::INLINEASM));

        std::string unique_part(
            preCallSymbol->getName().data() + std::size(preName) - 1);

        std::ostringstream inlineAsm;
        inlineAsm
            << "JMP " << preCallSymbol->getName().data() << "\n\t"
            << ".align " << DRTI_RETALIGN << "\n\t"
            << "L_DRTI_STASH_" << unique_part << ":\n\t"
            << ".8byte " << DRTI_MAGIC << "\n\t"
            << "L_DRTI_STASH_END_" << unique_part << ":\n\t"
            << ".skip "
            << DRTI_RETALIGN << " - " << DRTI_STASH_BYTES
            << " - (" << postCallSymbol->getName().data() << " - "
            << preCallSymbol->getName().data() << "), 0x90\n\t";

        MCSymbol* asmSymbol = MF.getContext().getOrCreateSymbol(inlineAsm.str());
        MF.getContext().registerInlineAsmLabel(asmSymbol);

        const char* string = asmSymbol->getName().data();
        MIB.addExternalSymbol(string);

        // Precaution against later elision
        unsigned extraInfo = InlineAsm::Extra_HasSideEffects;

        MIB.addImm(extraInfo);
        MIB.getInstr()->setPostInstrSymbol(MF, preCallSymbol);
    }

    // TODO (if possible) convert the call into a push and a jump with
    // the return address set to a separate return thunk which has
    // zeroes (or some other non-plausible prefix) immediately before
    // the return address.

    sink.setPostInstrSymbol(MF, postCallSymbol);
}

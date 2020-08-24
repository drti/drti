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

    struct X86TargetMachineStub
    {
        X86TargetMachineStub(
            const llvm::Target* baseTarget,
            const Target &T, const Triple &TT, StringRef CPU,
            StringRef FS, const TargetOptions &Options,
            Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
            CodeGenOpt::Level OL, bool JIT);

        std::unique_ptr<LLVMTargetMachine> m_baseTM;
    };

    class X86DrtiTargetMachine : private X86TargetMachineStub, public LLVMTargetMachine
    {
    public:
        X86DrtiTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                             StringRef FS, const TargetOptions &Options,
                             Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                             CodeGenOpt::Level OL, bool JIT);

    private:
        const TargetSubtargetInfo *getSubtargetImpl(const Function &F) const override;
        TargetTransformInfo getTargetTransformInfo(const Function &F) override;
        TargetPassConfig *createPassConfig(PassManagerBase &PM) override;
        TargetLoweringObjectFile *getObjFileLowering() const override;
    };

    //! This is a dirty hack to get access to the protected member
    //! functions of TargetPassConfig via the invalid static_casts
    //! below. This is undefined behaviour, of course, but probably
    //! works since we don't override any virtual functions and are
    //! probably layout-compatible with the X86PassConfig
    //! implementation class
    class PublicTargetPassConfig : public TargetPassConfig
    {
    public:
        using TargetPassConfig::addPreISel;
        using TargetPassConfig::addMachineSSAOptimization;
        using TargetPassConfig::addILPOpts;
        using TargetPassConfig::addPreRegAlloc;
        using TargetPassConfig::createTargetRegisterAllocator;
        using TargetPassConfig::addFastRegAlloc;
        using TargetPassConfig::addOptimizedRegAlloc;
        using TargetPassConfig::addPreRewrite;
        using TargetPassConfig::addPostRewrite;
        using TargetPassConfig::addPostRegAlloc;
        using TargetPassConfig::addMachineLateOptimization;
        using TargetPassConfig::addPreSched2;
        using TargetPassConfig::addGCPasses;
        using TargetPassConfig::addBlockPlacement;
        using TargetPassConfig::addPreEmitPass;
        using TargetPassConfig::addPreEmitPass2;
        using TargetPassConfig::createRegAllocPass;
        using TargetPassConfig::addRegAssignmentFast;
        using TargetPassConfig::addRegAssignmentOptimized;
    };

#define DELEGATE_0(RET_TYPE, NAME, CONST)            \
    RET_TYPE NAME() CONST override                   \
    {                                                \
        return static_cast<PublicTargetPassConfig&>( \
            *m_impl).NAME();                         \
    }

#define DELEGATE_1(RET_TYPE, NAME, PARAM_TYPE, CONST) \
    RET_TYPE NAME(PARAM_TYPE p) CONST override        \
    {                                                 \
        return static_cast<PublicTargetPassConfig&>(  \
            *m_impl).NAME(p);                         \
    }

    class X86DrtiPassConfig : public TargetPassConfig
    {
        std::unique_ptr<TargetPassConfig> m_impl;

    public:
        X86DrtiPassConfig(
            LLVMTargetMachine&,
            PassManagerBase&,
            std::unique_ptr<TargetPassConfig> impl);

        DELEGATE_1(
            ScheduleDAGInstrs*,
            createMachineScheduler,
            MachineSchedContext*,
            const)

        DELEGATE_1(
            ScheduleDAGInstrs*,
            createPostMachineScheduler,
            MachineSchedContext*,
            const)

        DELEGATE_0(void, addIRPasses, )
        DELEGATE_0(bool, addInstSelector, )
        DELEGATE_0(bool, addIRTranslator, )
        DELEGATE_0(bool, addLegalizeMachineIR, )
        DELEGATE_0(bool, addRegBankSelect, )
        DELEGATE_0(bool, addGlobalInstructionSelect, )
        DELEGATE_0(bool, addILPOpts, )
        DELEGATE_0(bool, addPreISel, )
        DELEGATE_0(void, addMachineSSAOptimization, )
        void addPreRegAlloc() override;
        DELEGATE_0(void, addPostRegAlloc, )
        DELEGATE_0(void, addPreEmitPass, )
        DELEGATE_0(void, addPreEmitPass2, )
        DELEGATE_0(void, addPreSched2, )

        DELEGATE_0(
            std::unique_ptr<CSEConfigBase>,
            getCSEConfig,
            const)

        // These aren't currently needed for X86PassConfig but that
        // could change. However we can't delegate (e.g.)
        // addMachinePasses because then it would call things like
        // addPreRegAlloc on *m_impl instead of *this.

        // DELEGATE_1(
        //     FunctionPass*,
        //     createTargetRegisterAllocator,
        //     bool, )
        // DELEGATE_1(
        //     FunctionPass*,
        //     createRegAllocPass,
        //     bool, )
        // DELEGATE_0(void, addFastRegAlloc, )
        // DELEGATE_0(void, addOptimizedRegAlloc, )
        // DELEGATE_0(bool, addPreRewrite, )
        // DELEGATE_0(void, addPostRewrite, )
        // DELEGATE_0(void, addMachineLateOptimization, )
        // DELEGATE_0(bool, addGCPasses, )
        // DELEGATE_0(void, addBlockPlacement, )
        // DELEGATE_0(bool, addRegAssignmentFast, )
        // DELEGATE_0(bool, addRegAssignmentOptimized, )

        // DELEGATE_0(void, addCodeGenPrepare, )
        // DELEGATE_0(void, addISelPrepare, )
        // DELEGATE_0(void, addPreLegalizeMachineIR, )
        // DELEGATE_0(void, addPreRegBankSelect, )
        // DELEGATE_0(void, addMachinePasses, )
        // DELEGATE_0(bool, reportDiagnosticWhenGlobalISelFallback, const)
        // DELEGATE_0(bool, isGISelCSEEnabled, const)
    };
}

char drti::X86DrtiTreenodePass::ID = 0;

#define REGISTER( SUFFIX, ALLOC ) \
    TargetRegistry::Register ## SUFFIX(drtiTarget, ALLOC)

#define AUTO_REGISTER( SUFFIX )                             \
    struct AutoRegister ## SUFFIX {                         \
        static SUFFIX* delegate() {                         \
            return x86BaseTarget->create ## SUFFIX();       \
        }                                                   \
    };                                                      \
    REGISTER( SUFFIX, AutoRegister ## SUFFIX :: delegate)

#define AUTO_REGISTER1_RET( SUFFIX, RET_TYPE, PARAM_TYPE_1 )      \
    struct AutoRegister ## SUFFIX {                               \
        static RET_TYPE delegate(PARAM_TYPE_1 p1) {               \
            return x86BaseTarget->create ## SUFFIX(pconvert(p1)); \
        }                                                         \
    };                                                            \
    REGISTER( SUFFIX, AutoRegister ## SUFFIX :: delegate)

#define AUTO_REGISTER1( SUFFIX, PARAM_TYPE_1 )          \
    AUTO_REGISTER1_RET( SUFFIX, SUFFIX*, PARAM_TYPE_1 )

#define AUTO_REGISTER2_RET( SUFFIX, RET_TYPE, PARAM_TYPE_1, PT_2 )      \
    struct AutoRegister ## SUFFIX {                                     \
        static RET_TYPE delegate(PARAM_TYPE_1 p1, PT_2 p2) {            \
            return x86BaseTarget->create ## SUFFIX(                     \
                pconvert(p1), p2);                                      \
        }                                                               \
    };                                                                  \
    REGISTER( SUFFIX, AutoRegister ## SUFFIX :: delegate)

#define AUTO_REGISTER2( SUFFIX, PARAM_TYPE_1, PT_2 )            \
    AUTO_REGISTER2_RET( SUFFIX, SUFFIX*, PARAM_TYPE_1, PT_2 )

#define AUTO_REGISTER3( SUFFIX, PARAM_TYPE_1, PT_2, PT_3 )              \
    struct AutoRegister ## SUFFIX {                                     \
        static SUFFIX* delegate(PARAM_TYPE_1 p1, PT_2 p2, PT_3 p3) {    \
            return x86BaseTarget->create ## SUFFIX(                     \
                pconvert(p1), p2, p3);                                  \
        }                                                               \
    };                                                                  \
    REGISTER( SUFFIX, AutoRegister ## SUFFIX :: delegate)

#define AUTO_REGISTER4_RET( \
    SUFFIX, RET_TYPE, PARAM_TYPE_1, PT_2, PT_3, PT_4 )            \
    struct AutoRegister ## SUFFIX {                               \
        static RET_TYPE delegate(                                 \
            PARAM_TYPE_1 p1, PT_2 p2, PT_3 p3, PT_4 p4) {         \
            return x86BaseTarget->create ## SUFFIX(               \
                p1, p2, p3, p4);                                  \
        }                                                         \
    };                                                            \
    REGISTER( SUFFIX, AutoRegister ## SUFFIX :: delegate)

#define AUTO_REGISTER_MC_ASM_BACKEND(                           \
    PARAM_TYPE_1, PT_2, PT_3, PT_4 )                            \
    struct AutoRegisterMCAsmBackend {                           \
        static MCAsmBackend* delegate(                          \
            PARAM_TYPE_1 p1, PT_2 p2, PT_3 p3, PT_4 p4) {       \
            return x86BaseTarget->createMCAsmBackend(           \
                p2, p3, p4);                                    \
        }                                                       \
    };                                                          \
    REGISTER( MCAsmBackend, AutoRegisterMCAsmBackend::delegate)

#define AUTO_REGISTER5( \
    SUFFIX, PARAM_TYPE_1, PT_2, PT_3, PT_4, PT_5 )                      \
    struct AutoRegister ## SUFFIX {                                     \
        static SUFFIX* delegate(                                        \
            PARAM_TYPE_1 p1, PT_2 p2, PT_3 p3, PT_4 p4, PT_5 p5) {      \
            return x86BaseTarget->create ## SUFFIX(                     \
                p1, p2, p3, p4, p5);                                    \
        }                                                               \
    };                                                                  \
    REGISTER( SUFFIX, AutoRegister ## SUFFIX :: delegate)


#define CHECK_AND_REGISTER( SUFFIX, ALLOC )     \
    do {                                        \
        if(x86BaseTarget->has ## SUFFIX) {      \
            REGISTER(SUFFIX, ALLOC);            \
        }                                       \
    } while(0)

namespace
{
    const llvm::Target* x86BaseTarget = nullptr;

    std::string pconvert(const Triple& triple)
    {
        return triple.getTriple();
    }

    template<typename T> T* pconvert(T* p)
    {
        return p;
    }

    template<typename T> T& pconvert(T& p)
    {
        return p;
    }

    struct r { r(); } registrar;
    r::r()
    {
        LLVMInitializeX86TargetInfo();
        LLVMInitializeX86Target();

        std::string error;
        x86BaseTarget = TargetRegistry::lookupTarget(
            "x86_64-unknown-linux-gnu", error);

        if(!x86BaseTarget)
        {
            throw std::runtime_error(
                "drti-target unable to find base target: " + error);
        }

        static Target drtiTarget;
        // HACK! Copy all the setup constructors we don't care about
        // verbatim from the X86 target (assumes Target really is a
        // POD type, as claimed). Some of these details get
        // overwritten by RegisterTarget and RegisterTargetMachine,
        // but only if !drtiTarget.Name.
        constexpr size_t pastNameOffset = 4 * sizeof(void*);
        std::memcpy(
            reinterpret_cast<char*>(&drtiTarget) + pastNameOffset,
            reinterpret_cast<const char*>(x86BaseTarget) + pastNameOffset,
            sizeof(Target) - pastNameOffset);

        RegisterTarget<Triple::UnknownArch, /*HasJIT=*/true> X(
            drtiTarget,
            "x86_64_drti",
            "64-bit X86: EM64T and AMD64 with DRTI support",
            "X86");

        RegisterTargetMachine<drti::X86DrtiTargetMachine> Y(drtiTarget);
        // RegisterMCAsmInfoFn Z(drtiTarget, drti::createX86DrtiMCAsmInfo);
        AUTO_REGISTER(MCInstrInfo);
        AUTO_REGISTER1_RET(MCRegInfo, MCRegisterInfo*, const Triple &);
        AUTO_REGISTER3(MCSubtargetInfo, const Triple&, StringRef, StringRef);
        AUTO_REGISTER1(MCInstrAnalysis, const MCInstrInfo*);
        AUTO_REGISTER3(MCCodeEmitter, const MCInstrInfo&, const MCRegisterInfo&, MCContext&);
        // Ignore ObjectTargetStreamer from X86MCTargetDesc.cpp since
        // it's only needed for COFF and isn't directly accessible via
        // x86BaseTarget
        // AUTO_REGISTER2_RET(
        //     ObjectTargetStreamer, MCTargetStreamer*, MCStreamer&, const MCSubtargetInfo&);
        AUTO_REGISTER4_RET(
            AsmTargetStreamer, MCTargetStreamer*,
            MCStreamer&, formatted_raw_ostream&, MCInstPrinter*, bool);
        // Don't bother with createX86WinCOFFStreamer
        AUTO_REGISTER5(
            MCInstPrinter, const Triple&, unsigned,
            const MCAsmInfo&, const MCInstrInfo&, const MCRegisterInfo&);

        AUTO_REGISTER2(MCRelocationInfo, const Triple&, MCContext&);

        if(x86BaseTarget->hasMCAsmBackend())
        {
            AUTO_REGISTER_MC_ASM_BACKEND(
                const Target&, const MCSubtargetInfo&,
                const MCRegisterInfo&, const MCTargetOptions&);
        }
    }
}

drti::X86TargetMachineStub::X86TargetMachineStub(
    const llvm::Target* baseTarget,
    const Target &T, const Triple &TT, StringRef CPU,
    StringRef FS, const TargetOptions &Options,
    Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
    CodeGenOpt::Level OL, bool JIT)
{
    // TODO is there a way to check cast safety here?
    m_baseTM.reset(
        static_cast<LLVMTargetMachine*>(
            baseTarget->createTargetMachine(
                "x86_64-unknown-linux-gnu",
                CPU, FS, Options, RM, CM, OL, JIT)));
}

drti::X86DrtiTargetMachine::X86DrtiTargetMachine(
    const Target &T, const Triple &TT, StringRef CPU,
    StringRef FS, const TargetOptions &Options,
    Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
    CodeGenOpt::Level OL, bool JIT) :

    X86TargetMachineStub(
        x86BaseTarget, T, TT, CPU, FS, Options, RM, CM, OL, JIT),

    // LLVMTargetMachine(const Target &T, StringRef DataLayoutString,
    //                 const Triple &TT, StringRef CPU, StringRef FS,
    //                 const TargetOptions &Options, Reloc::Model RM,
    //                 CodeModel::Model CM, CodeGenOpt::Level OL);
    LLVMTargetMachine(
        T,
        m_baseTM->createDataLayout().getStringRepresentation(),
        m_baseTM->getTargetTriple(),
        CPU, FS, Options,
        m_baseTM->getRelocationModel(),
        m_baseTM->getCodeModel(),
        OL)
{
    // Copy setup from X86TargetMachine constructor
    this->Options = m_baseTM->Options;
    setMachineOutliner(true);
    initAsmInfo();
}

const TargetSubtargetInfo *drti::X86DrtiTargetMachine::getSubtargetImpl(
    const Function &F) const
{
    return m_baseTM->getSubtargetImpl(F);
}

TargetTransformInfo drti::X86DrtiTargetMachine::getTargetTransformInfo(
    const Function &F)
{
    return m_baseTM->getTargetTransformInfo(F);
}

TargetPassConfig *drti::X86DrtiTargetMachine::createPassConfig(
    PassManagerBase &PM)
{
    return new X86DrtiPassConfig(
        *this, PM, std::unique_ptr<TargetPassConfig>(
            m_baseTM->createPassConfig(PM)));
}

TargetLoweringObjectFile *drti::X86DrtiTargetMachine::getObjFileLowering() const
{
    return m_baseTM->getObjFileLowering();
}

drti::X86DrtiPassConfig::X86DrtiPassConfig(
    LLVMTargetMachine &TM,
    PassManagerBase &pm,
    std::unique_ptr<TargetPassConfig> impl) :

    TargetPassConfig(TM, pm),
    m_impl(std::move(impl))
{
}

void drti::X86DrtiPassConfig::addPreRegAlloc()
{
    addPass(new X86DrtiTreenodePass);
    static_cast<PublicTargetPassConfig&>(*m_impl).addPreRegAlloc();
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

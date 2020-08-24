// -*- mode:c++ -*-
//
// Module runtime.cpp
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
// 2019/11/25   rmg     File creation
//

#include <drti/runtime.hpp>

#include "llvm/Analysis/InlineCost.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"

#include <drti/tree.hpp>
#include <drti/drti-common.hpp>

#include <iostream>

static std::ostream& log_stream(std::cerr);

namespace drti
{
    struct InternalCompilerError { };

    enum log_level : int { fatal, error, warn, info, trace, debug };
    struct runtime_config
    {
        int log_level = log_level::info;
    };

    bool abi_ok(int caller_abi);
    void maybe_log_treenode(treenode* node);
    void maybe_log_error(
        const landing_site&, const char* context, const char* message);
    void compile_treenode(treenode* node);

    runtime_config config;

    struct ReflectedModule
    {
        ReflectedModule(llvm::LLVMContext&, landing_site&);

        std::unique_ptr<llvm::Module> readModule(llvm::LLVMContext&);
        llvm::Function* callsite_function();
        void globalsMap(
            llvm::orc::SymbolMap& map,
            llvm::orc::MangleAndInterner&,
            llvm::StringRef excludedFunctionName);

        landing_site& m_landing_site;
        reflect& m_self;
        std::unique_ptr<llvm::Module> m_ownModule;
        llvm::Module* m_module;
    };

    class TreenodeCompiler
    {
    public:
        TreenodeCompiler(treenode* node);
        void* compile();

    private:
        std::unique_ptr<llvm::orc::LLJIT> createJit();
        void linkModules();
        void reprocess(llvm::Function*, ReflectedModule&, const static_callsite&);
        void reprocess(llvm::CallBase* callInst, ReflectedModule& leaf);

        void optimize();

        treenode* m_node;

        llvm::orc::ThreadSafeContext m_thread_safe_context;
        llvm::orc::ThreadSafeContext::Lock m_lock;
        llvm::LLVMContext& m_context;

        ReflectedModule m_leaf;
        ReflectedModule m_caller;

        std::unique_ptr<llvm::orc::LLJIT> m_jit;
    };
}

static int oneTimeInit()
{
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    return 0;
}

static llvm::orc::ThreadSafeContext llvmContext()
{
    using namespace llvm;
    static int dummy = oneTimeInit();
    static_cast<void>(dummy);
    static orc::ThreadSafeContext tsc(std::make_unique<LLVMContext>());
    return tsc;
}

bool drti::abi_ok(int caller_abi)
{
    if(caller_abi != abi_version)
    {
        if(config.log_level >= log_level::error)
        {
            log_stream
                << "DRTI ABI mismatch client "
                << caller_abi
                << " != runtime "
                << abi_version
                << std::endl;
        }
        return false;
    }
    else
    {
        return true;
    }
}

#define HANDLE( LANDING, CONTEXT, ERROR )               \
    do                                                  \
    {                                                   \
        llvm::handleAllErrors(                          \
            ERROR,                                      \
            [this](llvm::ErrorInfoBase& EIB) {          \
                drti::maybe_log_error(                  \
                    LANDING, CONTEXT,                   \
                    EIB.message().c_str() );            \
            });                                         \
        throw InternalCompilerError();                  \
    } while(0)

#define CHECK_WRAPPER( LANDING, CONTEXT, WRAPPER )      \
    if(!WRAPPER)                                        \
    {                                                   \
        HANDLE(LANDING, CONTEXT, WRAPPER.takeError());  \
    }

#define CHECK_ERROR( LANDING, CONTEXT, ERROR )          \
    if(ERROR)                                           \
    {                                                   \
        HANDLE(LANDING, CONTEXT, std::move(ERROR));     \
    }

void drti::maybe_log_treenode(treenode* node)
{
    if(config.log_level >= log_level::info)
    {
        log_stream << "DRTI ";
        if(node->parent)
        {
            log_stream
                << node->parent->location.landing.total_called
                << " * "
                << node->parent->location.landing.global_name
                << " via "
                << node->parent->target;
        }
        else
        {
            log_stream << "(unknown)";
        }

        log_stream
            << " -> "
            << node->location.landing.total_called
            << " * "
            << node->location.landing.function_name
            << " "
            << node->location.total_calls
            << " visits via "
            << node->target
            << " -> "
            << node->chain_calls
            << " * "
            << node->landing->function_name
            << " ("
            << node->landing->total_called
            << " total)"
            << std::endl;
    }
}

void drti::maybe_log_error(
    const landing_site& landing, const char* context, const char* message)
{
    if(config.log_level >= log_level::error)
    {
        log_stream
            << "DRTI "
            << landing.function_name
            << " "
            << context
            << " "
            << message
            << "\n";
    }
}

void drti::inspect_treenode(treenode* node)
{
    if(!abi_ok(node->caller_abi_version))
    {
        return;
    }

    maybe_log_treenode(node);

    if(node->parent)
    {
        try
        {
            compile_treenode(node);
        }
        catch(const InternalCompilerError&)
        {
        }
    }
}

drti::ReflectedModule::ReflectedModule(
    llvm::LLVMContext& context, landing_site& site) :

    m_landing_site(site),
    m_self(*m_landing_site.self),
    m_ownModule(readModule(context)),
    m_module(m_ownModule.get())
{
}

std::unique_ptr<llvm::Module> drti::ReflectedModule::readModule(
    llvm::LLVMContext& context)
{
    assert(m_landing_site.self);

    llvm::StringRef string(m_self.module, m_self.module_size);

    auto buffer(
        llvm::MemoryBuffer::getMemBuffer(string, "bitcode", false));

    // Hmmm, using lazy leads to the assrtion failure:
    // Assertion `!NodePtr->isKnownSentinel()' failed.
    // deep inside JIT compilation in FPPassManager::runOnFunction
    // for _ZN4drti15static_callsiteD2Ev
    //
    // llvm::Expected<std::unique_ptr<llvm::Module>> module(
    //     llvm::getLazyBitcodeModule(*buffer, *context, true, false));
    // CHECK_WRAPPER(module, "getLazyBitcodeModule");

    llvm::Expected<std::unique_ptr<llvm::Module>> maybeModule(
        llvm::parseBitcodeFile(*buffer, context));

    CHECK_WRAPPER(m_landing_site, "parseBitcodeFile", maybeModule);

    if(config.log_level >= log_level::info)
    {
        log_stream
            << "DRTI module for "
            << m_landing_site.function_name
            << " of size "
            << m_self.module_size
            << "\n";
    }

    return std::move(*maybeModule);
}

llvm::Function* drti::ReflectedModule::callsite_function()
{
    llvm::Function* func = m_module->getFunction(m_landing_site.function_name);
    if(!func)
    {
        if(config.log_level >= log_level::error)
        {
            log_stream
                << "DRTI "
                << m_landing_site.function_name
                << " not found in bitcode. Globals dump follows:\n";

            for(llvm::Function& function: m_module->functions())
            {
                log_stream << "DRTI " << function.getName().str() << "\n";
            }
            for(llvm::GlobalVariable& global: m_module->globals())
            {
                log_stream << "DRTI " << global.getName().str() << "\n";
            }
        }
        throw InternalCompilerError();
    }

    return func;
}

void drti::ReflectedModule::globalsMap(
    llvm::orc::SymbolMap& map,
    llvm::orc::MangleAndInterner& mangler,
    llvm::StringRef excludedFunctionName)
{
    // We must process these in exactly the same order as the code
    // that populated the reflect.globals (see drti-decorate.cpp)
    size_t index = 0;

    auto addNext = [&](llvm::StringRef name) {
        if(index >= m_self.globals_size)
        {
            if(config.log_level >= log_level::error)
            {
                log_stream
                    << "DRTI "
                    << m_landing_site.function_name
                    << " module has "
                    << (index + 1)
                    << " globals but only "
                    << m_self.globals_size
                    << " stored addresses\n";
            }
            throw InternalCompilerError();
        }

        // TODO - check for invalid collisions
        llvm::JITEvaluatedSymbol address(
            reinterpret_cast<uintptr_t>(m_self.globals[index]),
            llvm::JITSymbolFlags::Exported);

        llvm::orc::SymbolStringPtr symbol = mangler(name);

        if(config.log_level >= log_level::debug)
        {
            log_stream
                << "DRTI "
                << (*symbol).str()
                << " runtime address "
                << reinterpret_cast<const void*>(address.getAddress())
                << "\n";
        }

        map[symbol] = address;

        ++index;
    };

    visit_listed_globals(
        *m_module,
        [&addNext](llvm::GlobalVariable& variable) {
            addNext(variable.getName());

            // Force "internal" variables to resolve against the
            // original copy compiled ahead-of-time and saved in the
            // reflected globals list. This is essential for static
            // initialisers to work and only be invoked once.
            //
            // TODO - we could add special handling for static
            // initialisation guard variables and completely elide
            // guard checks and init code for variables already
            // initialised at JIT time. Actually in general some
            // variables have only two states and we could convert
            // them to compile-time constants given enough knowledge.
            if(variable.hasLocalLinkage())
            {
                variable.setLinkage(
                    llvm::GlobalValue::AvailableExternallyLinkage);
            }
        });

    for(llvm::Function& function: m_module->functions())
    {
        // IMPORTANT - filtering here must match the same functions as
        // in collect_globals from drti-decorate.cpp
        if(function.isDeclaration() && !function.isIntrinsic())
        {
            if(function.getName() == excludedFunctionName)
            {
                if(config.log_level >= log_level::debug)
                {
                    log_stream
                        << "DRTI not mapping target function "
                        << excludedFunctionName.str()
                        << "\n";
                }
            }
            else
            {
                addNext(function.getName());
            }
        }
    }
}

drti::TreenodeCompiler::TreenodeCompiler(treenode* node) :
    m_node(node),
    m_thread_safe_context(llvmContext()),
    m_lock(m_thread_safe_context.getLock()),
    m_context(*m_thread_safe_context.getContext()),
    m_leaf(m_context, *m_node->landing),
    m_caller(m_context, m_node->location.landing),
    m_jit(createJit())
{
    llvm::orc::LLJIT& jit(*m_jit);

    // For symbols such as _Unwind_Resume
    jit.getExecutionSession().getMainJITDylib().setGenerator(
        llvm::cantFail(
            llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
                jit.getDataLayout().getGlobalPrefix())));

    llvm::orc::MangleAndInterner mangler(
        jit.getExecutionSession(), jit.getDataLayout());

    llvm::orc::SymbolMap globals_map;

    m_leaf.globalsMap(globals_map, mangler, llvm::StringRef(nullptr, 0));
    m_caller.globalsMap(globals_map, mangler, m_leaf.m_landing_site.function_name);

    llvm::cantFail(
        jit.getMainJITDylib().define(
            llvm::orc::absoluteSymbols(globals_map)));
}

std::unique_ptr<llvm::orc::LLJIT> drti::TreenodeCompiler::createJit()
{
    llvm::orc::JITTargetMachineBuilder jtmb(
        llvm::cantFail(llvm::orc::JITTargetMachineBuilder::detectHost()));
    // I think this controls machine code optimizations only (not the
    // IR->IR passes)
    jtmb.setCodeGenOptLevel(llvm::CodeGenOpt::Aggressive);
    // Currently this produces far too much output to be useful. Maybe
    // the compilation is not sufficiently lazy
    // jtmb.getOptions().PrintMachineCode = 1;

    // Code and data can be very far apart
    jtmb.setCodeModel(llvm::CodeModel::Large);

    llvm::orc::LLJITBuilder bs;
    bs.setJITTargetMachineBuilder(jtmb);

    auto maybeJit(bs.create());

    CHECK_WRAPPER(m_node->location.landing, "LLJIT::Create", maybeJit);

    return std::move(*maybeJit);
}

void drti::TreenodeCompiler::linkModules()
{
    if(config.log_level >= log_level::debug)
    {
        llvm::raw_os_ostream stream(std::cerr);
        std::unique_ptr<llvm::ModulePass> printer(
            llvm::createPrintModulePass(
                stream, "------- drti linking -------"));
        printer->runOnModule(*m_caller.m_module);
        printer->runOnModule(*m_leaf.m_module);
    }

    llvm::Linker linker(*m_caller.m_module);
    if(linker.linkInModule(
           std::move(m_leaf.m_ownModule), llvm::Linker::LinkOnlyNeeded))
    {
        maybe_log_error(
            m_leaf.m_landing_site,
            "TreenodeCompiler::createJit",
            "Linking failed");

        throw InternalCompilerError();
    }

    // m_leaf.m_ownModule is empty now, and we redirect its non-owned
    // pointer here
    m_leaf.m_module = m_caller.m_module;
}

void drti::TreenodeCompiler::reprocess(
    llvm::CallBase* callInst, ReflectedModule& leaf)
{
    // Split the existing block
    // BB1:
    //   xxx
    //   original = call value(...)
    //   yyy
    //
    // like this:
    // BB1:
    //   xxx
    //   matches = value == known
    //   br i1 matches, BB2, BB3
    // BB2:
    //   res1 = call inlinable_function(...)
    //   br BB4
    // BB3:
    //   original = call value(...)
    //   br BB4
    // BB4:
    //   res = phi [ res1, BB2 ], [ original, BB3 ]
    //   yyy
        

    llvm::IRBuilder<> builder(callInst);

    llvm::Type* int64 = llvm::IntegerType::get(m_context, 64);

    llvm::Value* target = builder.CreatePointerCast(
        callInst->getCalledOperand(), int64, "castTarget");

    llvm::Constant* knownTarget =
        llvm::ConstantInt::get(
            int64, reinterpret_cast<uintptr_t>(m_node->target));

    llvm::Value* matches = builder.CreateICmpEQ(
        target, knownTarget, "matches");

    llvm::BasicBlock* bb1 = callInst->getParent();
    llvm::BasicBlock* bb3 = bb1->splitBasicBlock(callInst, "drti_bb3");
    llvm::BasicBlock* bb4 = bb3->splitBasicBlock(
        callInst->getNextNode(), "drti_bb4");
    llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(
        m_context, "drti_bb2", bb1->getParent(), bb3);
    // TODO - instrument (redecorate) the slow path in bb3

    // Remove the unconditional branch inserted by splitBasicBlock
    builder.SetInsertPoint(bb1, bb1->back().eraseFromParent());
    // TODO - add branch weights
    builder.CreateCondBr(matches, bb2, bb3);

    // The inlinable function call
    builder.SetInsertPoint(bb2);
    llvm::SmallVector<llvm::Value*, 20> args;
    for(llvm::Use& argUse: callInst->arg_operands())
    {
        args.push_back(argUse.get());
    }
    llvm::CallBase* directCall = builder.CreateCall(
        leaf.callsite_function(), args);
    builder.CreateBr(bb4);

    llvm::Type* resultType = callInst->getFunctionType()->getReturnType();
    if(resultType != directCall->getFunctionType()->getReturnType())
    {
        maybe_log_error(
            leaf.m_landing_site,
            "TreenodeCompiler::reprocess",
            "Result type mismatch");
        throw InternalCompilerError();
    }

    if(!resultType->isVoidTy())
    {
        // Create a PHI node for the results from the two branches
        builder.SetInsertPoint(bb4, bb4->begin());
        llvm::PHINode* resultPhi = builder.CreatePHI(
            resultType, 2, "drti_merged_result");
        // Replace any uses of the original return value with the PHI node
        callInst->replaceAllUsesWith(resultPhi);
        resultPhi->addIncoming(directCall, bb2);
        resultPhi->addIncoming(callInst, bb3);
    }

    // Clean up the builder in case of any further insertions by our
    // caller
    builder.SetInsertPoint(bb4);
}

//! For calls via a function pointer we add code to check the pointer
//! value before using the direct call determined at runtime (fast
//! path), and call via the pointer otherwise (slow path). Currently
//! only handles a single call site
void drti::TreenodeCompiler::reprocess(
    llvm::Function* function, ReflectedModule& leaf, const static_callsite& callsite)
{
    // TODO - handle multiple callsites. Probably our landing_site
    // needs references to all its contained callsites so we can
    // reprocess all of them at once. Combinations could explode
    // with all the possible treenodes from each callsite
    unsigned call_number = 0;
    for(llvm::BasicBlock& block: *function)
    {
        for(llvm::Instruction& instruction: block)
        {
            auto callInst = llvm::dyn_cast<llvm::CallBase>(&instruction);
            if(callInst)
            {
                llvm::Function* calledFunction(callInst->getCalledFunction());
                if(config.log_level >= log_level::trace)
                {
                    log_stream
                        << "DRTI "
                        << function->getName().str()
                        << " call_number "
                        << call_number
                        << " "
                        << (calledFunction ?
                            calledFunction->getName().str() :
                            std::string("pointer"))
                        << "\n";
                }

                if(call_number == callsite.call_number)
                {
                    // Currently we only need to reprocess calls via
                    // function pointers, so not those direct to a
                    // function global. TODO - optimise this ahead of time
                    if(!calledFunction)
                    {
                        reprocess(callInst, leaf);
                    }
                    return;
                }
                ++call_number;
            }
        }
    }
}

void drti::TreenodeCompiler::optimize()
{
    llvm::PassManagerBuilder pmb;

    // We like inlining a lot. The normal default cost threshold is
    // 225
    pmb.Inliner = llvm::createFunctionInliningPass(1000);
    pmb.OptLevel = 3;

    llvm::legacy::PassManager mpm;

    pmb.populateModulePassManager(mpm);

    mpm.run(*m_caller.m_module);

    llvm::legacy::FunctionPassManager fpm(m_caller.m_module);
    pmb.populateFunctionPassManager(fpm);

    // We assume that the leaf function was already optimised during
    // ahead-of-time compilation, so there is currently not much to be
    // gained by re-optimizing it now. It might well have been inlined
    // and deleted by the module passes anyway
    // fpm.run(*m_leaf.callsite_function());
    fpm.run(*m_caller.callsite_function());
}

void* drti::TreenodeCompiler::compile()
{
    llvm::orc::LLJIT& jit(*m_jit);

    llvm::Function* caller_func = m_caller.callsite_function();

    if(config.log_level >= log_level::info)
    {
        log_stream
            << "DRTI attempting to inline call from "
            << m_caller.m_landing_site.function_name
            << " to "
            << m_leaf.m_landing_site.function_name
            << std::endl;
    }

    // Make leaf function externally visible so it can be linked for
    // inlining.
    m_leaf.callsite_function()->setLinkage(llvm::GlobalValue::LinkOnceAnyLinkage);
    // Why is this necessary, and why isn't the loop in
    // fpointer_main.cpp do_call being optimized away after increment1
    // is inlined?
    m_leaf.callsite_function()->addFnAttr(llvm::Attribute::AlwaysInline);

    // Make caller extern so we can get its address.  Must do this
    // before the addIRModule since that scans the module immediately
    caller_func->setLinkage(llvm::GlobalValue::ExternalLinkage);

    // This resets the m_leaf.m_ownModule unique_ptr and redirects
    // m_leaf.m_module
    linkModules();

    reprocess(caller_func, m_leaf, m_node->location);

    if(config.log_level >= log_level::trace)
    {
        llvm::raw_os_ostream stream(std::cerr);
        std::unique_ptr<llvm::ModulePass> printer(
            llvm::createPrintModulePass(
                stream, "------- pre-optimize -------"));
        printer->runOnModule(*m_caller.m_module);
    }

    optimize();

    llvm::Error bad = jit.addIRModule(
        llvm::orc::ThreadSafeModule(
            std::move(m_caller.m_ownModule), m_thread_safe_context));

    CHECK_ERROR(m_node->location.landing, "addIRModule", bad);

    if(config.log_level >= log_level::trace)
    {
        llvm::raw_os_ostream stream(log_stream);
        std::unique_ptr<llvm::FunctionPass> printer(
            llvm::createPrintFunctionPass(
                stream, "---- drti compiling ----"));
        printer->runOnFunction(*caller_func);
    }

    // TODO - add verifier pass
    auto maybeAddress = jit.lookup(
        m_caller.m_landing_site.function_name);

    CHECK_WRAPPER(m_caller.m_landing_site, "jit.lookup caller", maybeAddress);

    void* result = reinterpret_cast<void*>(maybeAddress->getAddress());
    if(config.log_level >= log_level::trace)
    {
        log_stream
            << "DRTI "
            << m_caller.m_landing_site.function_name
            << " compiled address "
            << result
            << std::endl;
    }

    return result;
}

void drti::compile_treenode(treenode* node)
{
    // LEAK the entire thing to prevent cleanup of the generated
    // machine code. TODO - save just the machine code
    TreenodeCompiler& treenode_compiler(*new TreenodeCompiler(node));

    // Redirect function pointer to the new machine code
    node->parent->resolvedTarget = treenode_compiler.compile();
}

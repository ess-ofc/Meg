/*
 * ======================================
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (c) 2026 Elizeu S. Souza
 * ======================================
 */

#include <megc/cgen.h>

#include <megc/diagno.h>

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdio.h>
#include <stdlib.h>

struct cgen {
   LLVMContextRef ctx;
   LLVMModuleRef mod;
};

static void gendecl(struct cgen *self, struct mdecl *d);

static LLVMTypeRef gentype(
   struct cgen *self,
   mqtype qt
) {
   auto t = mqtype_get(qt);

   switch (t->kind) {
   case mTYPE_INVAL:
      madeus("Invalid type.");
   case mTYPE_UNA:
      auto u = &t->as.una;
      if (u->decl->as.type.kind == mTYPEDEF_DEF) {
         return LLVMIntTypeInContext(
            self->ctx,
            u->decl->as.type.as.def.size * 8
         );
      }

      madeus("Una definition not supported.");
   default:
      madeus("Type not supported.");
   }
}

static LLVMValueRef genexpr(
   struct cgen *self,
   struct mexpr *e,
   LLVMBuilderRef builder,
   LLVMBasicBlockRef bb
) {
   switch (e->kind) {
   case mDECL_INVAL:
      madeus("Invalid expression.");
   case mEXPR_BIN_OP:
      auto bin_op = &e->as.bin_op;
      auto lhs = genexpr(self, bin_op->lhs, builder, bb);
      auto rhs = genexpr(self, bin_op->rhs, builder, bb);

      switch (bin_op->kind) {
      case mBIN_OP_ADD:
         return LLVMBuildAdd(builder, lhs, rhs, "");
      case mBIN_OP_SUB:
         return LLVMBuildSub(builder, lhs, rhs, "");
      case mBIN_OP_MUL:
         return LLVMBuildMul(builder, lhs, rhs, "");
      default:
         madeus("Unsupported binary operator.");
      }
   case mEXPR_LIT:
      auto lit = &e->as.lit;
      switch (lit->kind) {
      case mLIT_INTEGER:
         return LLVMConstInt(gentype(self, e->type), lit->as.i, true);
      default:
         madeus("Unsupported literal.");
      }
   default:
      madeus("Unsupported expression.");
   }
}

static void gendecl(
   struct cgen *self,
   struct mdecl *d
) {
   switch (d->kind) {
   default:
   case mDECL_INVAL:
      madeus("Invalid declaration.");
   case mDECL_TYPE:
      break;
   case mDECL_FUNC:
      auto f = &d->as.func;
      LLVMTypeRef parms[f->scope->count];
      size_t parmc = 0;
      for (auto p = f->scope->fst; p; p = p->next) {
         parms[parmc++] = gentype(self, p->decl->type);
      }

      auto func = LLVMAddFunction(
         self->mod,
         d->id,
         LLVMFunctionType(
            gentype(self, d->type),
            parms,
            parmc,
            false
         )
      );
      auto bb = LLVMAppendBasicBlockInContext(self->ctx, func, "main");
      auto builder = LLVMCreateBuilderInContext(self->ctx);
      LLVMPositionBuilderAtEnd(builder, bb);
      LLVMBuildRet(builder, genexpr(self, f->expr, builder, bb));
      break;
   }
}

void mcgen_genunit(struct munit *unit) {
   LLVMContextRef ctx = LLVMContextCreate();
   LLVMModuleRef mod = LLVMModuleCreateWithNameInContext(unit->name, ctx);

   struct cgen self = {
      .ctx = ctx,
      .mod = mod
   };

   auto d = unit->scope->fst;
   while (d) {
      gendecl(&self, d->decl);
      d = d->next;
   }

   char *err = nullptr;
   if (LLVMVerifyModule(mod, LLVMAbortProcessAction, &err)) {
      merro("LLVM: %s", err);
      LLVMDisposeMessage(err);
      goto dispose;
   }

   LLVMInitializeNativeTarget();
   LLVMInitializeNativeAsmPrinter();
   LLVMInitializeNativeAsmParser();

   auto triple = LLVMGetDefaultTargetTriple();
   LLVMSetTarget(mod, triple);

   err = nullptr;
   LLVMTargetRef target;
   if (LLVMGetTargetFromTriple(triple, &target, &err)) {
      merro("LLVM: %s", err);
      LLVMDisposeMessage(err);
      LLVMDisposeMessage(triple);
      goto dispose;
   }

   auto tm = LLVMCreateTargetMachine(
      target,
      triple,
      "generic",
      "",
      LLVMCodeGenLevelDefault,
      LLVMRelocPIC,
      LLVMCodeModelDefault
   );

   auto data = LLVMCreateTargetDataLayout(tm);
   LLVMSetModuleDataLayout(mod, data);

   err = nullptr;
   if (LLVMTargetMachineEmitToFile(tm, mod, "out.o", LLVMObjectFile, &err)) {
      merro("LLVM: %s", err);
      LLVMDisposeMessage(err);
      LLVMDisposeTargetData(data);
      LLVMDisposeTargetMachine(tm);
      LLVMDisposeMessage(triple);
   }

   system("cc -fuse-ld=lld out.o -o main");

   LLVMDisposeTargetData(data);
   LLVMDisposeTargetMachine(tm);
   LLVMDisposeMessage(triple);

dispose:
   LLVMDisposeModule(mod);
   LLVMContextDispose(ctx);
}

//==-- RetainCountChecker.cpp - Checks for leaks and other issues -*- C++ -*--//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the methods for RetainCountChecker, which implements
//  a reference count checker for Core Foundation and Cocoa on (Mac OS X).
//
//===----------------------------------------------------------------------===//

#include "RetainCountChecker.h"

using namespace clang;
using namespace ento;
using namespace retaincountchecker;
using llvm::StrInStrNoCase;

REGISTER_MAP_WITH_PROGRAMSTATE(RefBindings, SymbolRef, RefVal)

namespace clang {
namespace ento {
namespace retaincountchecker {

const RefVal *getRefBinding(ProgramStateRef State, SymbolRef Sym) {
  return State->get<RefBindings>(Sym);
}

ProgramStateRef setRefBinding(ProgramStateRef State, SymbolRef Sym,
                                     RefVal Val) {
  assert(Sym != nullptr);
  return State->set<RefBindings>(Sym, Val);
}

ProgramStateRef removeRefBinding(ProgramStateRef State, SymbolRef Sym) {
  return State->remove<RefBindings>(Sym);
}

} // end namespace retaincountchecker
} // end namespace ento
} // end namespace clang

void RefVal::print(raw_ostream &Out) const {
  if (!T.isNull())
    Out << "Tracked " << T.getAsString() << '/';

  switch (getKind()) {
    default: llvm_unreachable("Invalid RefVal kind");
    case Owned: {
      Out << "Owned";
      unsigned cnt = getCount();
      if (cnt) Out << " (+ " << cnt << ")";
      break;
    }

    case NotOwned: {
      Out << "NotOwned";
      unsigned cnt = getCount();
      if (cnt) Out << " (+ " << cnt << ")";
      break;
    }

    case ReturnedOwned: {
      Out << "ReturnedOwned";
      unsigned cnt = getCount();
      if (cnt) Out << " (+ " << cnt << ")";
      break;
    }

    case ReturnedNotOwned: {
      Out << "ReturnedNotOwned";
      unsigned cnt = getCount();
      if (cnt) Out << " (+ " << cnt << ")";
      break;
    }

    case Released:
      Out << "Released";
      break;

    case ErrorDeallocNotOwned:
      Out << "-dealloc (not-owned)";
      break;

    case ErrorLeak:
      Out << "Leaked";
      break;

    case ErrorLeakReturned:
      Out << "Leaked (Bad naming)";
      break;

    case ErrorUseAfterRelease:
      Out << "Use-After-Release [ERROR]";
      break;

    case ErrorReleaseNotOwned:
      Out << "Release of Not-Owned [ERROR]";
      break;

    case RefVal::ErrorOverAutorelease:
      Out << "Over-autoreleased";
      break;

    case RefVal::ErrorReturnedNotOwned:
      Out << "Non-owned object returned instead of owned";
      break;
  }

  switch (getIvarAccessHistory()) {
  case IvarAccessHistory::None:
    break;
  case IvarAccessHistory::AccessedDirectly:
    Out << " [direct ivar access]";
    break;
  case IvarAccessHistory::ReleasedAfterDirectAccess:
    Out << " [released after direct ivar access]";
  }

  if (ACnt) {
    Out << " [autorelease -" << ACnt << ']';
  }
}

namespace {
class StopTrackingCallback final : public SymbolVisitor {
  ProgramStateRef state;
public:
  StopTrackingCallback(ProgramStateRef st) : state(std::move(st)) {}
  ProgramStateRef getState() const { return state; }

  bool VisitSymbol(SymbolRef sym) override {
    state = state->remove<RefBindings>(sym);
    return true;
  }
};
} // end anonymous namespace

//===----------------------------------------------------------------------===//
// Handle statements that may have an effect on refcounts.
//===----------------------------------------------------------------------===//

void RetainCountChecker::checkPostStmt(const BlockExpr *BE,
                                       CheckerContext &C) const {

  // Scan the BlockDecRefExprs for any object the retain count checker
  // may be tracking.
  if (!BE->getBlockDecl()->hasCaptures())
    return;

  ProgramStateRef state = C.getState();
  auto *R = cast<BlockDataRegion>(C.getSVal(BE).getAsRegion());

  BlockDataRegion::referenced_vars_iterator I = R->referenced_vars_begin(),
                                            E = R->referenced_vars_end();

  if (I == E)
    return;

  // FIXME: For now we invalidate the tracking of all symbols passed to blocks
  // via captured variables, even though captured variables result in a copy
  // and in implicit increment/decrement of a retain count.
  SmallVector<const MemRegion*, 10> Regions;
  const LocationContext *LC = C.getLocationContext();
  MemRegionManager &MemMgr = C.getSValBuilder().getRegionManager();

  for ( ; I != E; ++I) {
    const VarRegion *VR = I.getCapturedRegion();
    if (VR->getSuperRegion() == R) {
      VR = MemMgr.getVarRegion(VR->getDecl(), LC);
    }
    Regions.push_back(VR);
  }

  state =
    state->scanReachableSymbols<StopTrackingCallback>(Regions.data(),
                                    Regions.data() + Regions.size()).getState();
  C.addTransition(state);
}

void RetainCountChecker::checkPostStmt(const CastExpr *CE,
                                       CheckerContext &C) const {
  const ObjCBridgedCastExpr *BE = dyn_cast<ObjCBridgedCastExpr>(CE);
  if (!BE)
    return;

  ArgEffect AE = IncRef;

  switch (BE->getBridgeKind()) {
    case OBC_Bridge:
      // Do nothing.
      return;
    case OBC_BridgeRetained:
      AE = IncRef;
      break;
    case OBC_BridgeTransfer:
      AE = DecRefBridgedTransferred;
      break;
  }

  ProgramStateRef state = C.getState();
  SymbolRef Sym = C.getSVal(CE).getAsLocSymbol();
  if (!Sym)
    return;
  const RefVal* T = getRefBinding(state, Sym);
  if (!T)
    return;

  RefVal::Kind hasErr = (RefVal::Kind) 0;
  state = updateSymbol(state, Sym, *T, AE, hasErr, C);

  if (hasErr) {
    // FIXME: If we get an error during a bridge cast, should we report it?
    return;
  }

  C.addTransition(state);
}

void RetainCountChecker::processObjCLiterals(CheckerContext &C,
                                             const Expr *Ex) const {
  ProgramStateRef state = C.getState();
  const ExplodedNode *pred = C.getPredecessor();
  for (const Stmt *Child : Ex->children()) {
    SVal V = pred->getSVal(Child);
    if (SymbolRef sym = V.getAsSymbol())
      if (const RefVal* T = getRefBinding(state, sym)) {
        RefVal::Kind hasErr = (RefVal::Kind) 0;
        state = updateSymbol(state, sym, *T, MayEscape, hasErr, C);
        if (hasErr) {
          processNonLeakError(state, Child->getSourceRange(), hasErr, sym, C);
          return;
        }
      }
  }

  // Return the object as autoreleased.
  //  RetEffect RE = RetEffect::MakeNotOwned(RetEffect::ObjC);
  if (SymbolRef sym =
        state->getSVal(Ex, pred->getLocationContext()).getAsSymbol()) {
    QualType ResultTy = Ex->getType();
    state = setRefBinding(state, sym,
                          RefVal::makeNotOwned(RetEffect::ObjC, ResultTy));
  }

  C.addTransition(state);
}

void RetainCountChecker::checkPostStmt(const ObjCArrayLiteral *AL,
                                       CheckerContext &C) const {
  // Apply the 'MayEscape' to all values.
  processObjCLiterals(C, AL);
}

void RetainCountChecker::checkPostStmt(const ObjCDictionaryLiteral *DL,
                                       CheckerContext &C) const {
  // Apply the 'MayEscape' to all keys and values.
  processObjCLiterals(C, DL);
}

void RetainCountChecker::checkPostStmt(const ObjCBoxedExpr *Ex,
                                       CheckerContext &C) const {
  const ExplodedNode *Pred = C.getPredecessor();
  ProgramStateRef State = Pred->getState();

  if (SymbolRef Sym = Pred->getSVal(Ex).getAsSymbol()) {
    QualType ResultTy = Ex->getType();
    State = setRefBinding(State, Sym,
                          RefVal::makeNotOwned(RetEffect::ObjC, ResultTy));
  }

  C.addTransition(State);
}

void RetainCountChecker::checkPostStmt(const ObjCIvarRefExpr *IRE,
                                       CheckerContext &C) const {
  Optional<Loc> IVarLoc = C.getSVal(IRE).getAs<Loc>();
  if (!IVarLoc)
    return;

  ProgramStateRef State = C.getState();
  SymbolRef Sym = State->getSVal(*IVarLoc).getAsSymbol();
  if (!Sym || !dyn_cast_or_null<ObjCIvarRegion>(Sym->getOriginRegion()))
    return;

  // Accessing an ivar directly is unusual. If we've done that, be more
  // forgiving about what the surrounding code is allowed to do.

  QualType Ty = Sym->getType();
  RetEffect::ObjKind Kind;
  if (Ty->isObjCRetainableType())
    Kind = RetEffect::ObjC;
  else if (coreFoundation::isCFObjectRef(Ty))
    Kind = RetEffect::CF;
  else
    return;

  // If the value is already known to be nil, don't bother tracking it.
  ConstraintManager &CMgr = State->getConstraintManager();
  if (CMgr.isNull(State, Sym).isConstrainedTrue())
    return;

  if (const RefVal *RV = getRefBinding(State, Sym)) {
    // If we've seen this symbol before, or we're only seeing it now because
    // of something the analyzer has synthesized, don't do anything.
    if (RV->getIvarAccessHistory() != RefVal::IvarAccessHistory::None ||
        isSynthesizedAccessor(C.getStackFrame())) {
      return;
    }

    // Note that this value has been loaded from an ivar.
    C.addTransition(setRefBinding(State, Sym, RV->withIvarAccess()));
    return;
  }

  RefVal PlusZero = RefVal::makeNotOwned(Kind, Ty);

  // In a synthesized accessor, the effective retain count is +0.
  if (isSynthesizedAccessor(C.getStackFrame())) {
    C.addTransition(setRefBinding(State, Sym, PlusZero));
    return;
  }

  State = setRefBinding(State, Sym, PlusZero.withIvarAccess());
  C.addTransition(State);
}

void RetainCountChecker::checkPostCall(const CallEvent &Call,
                                       CheckerContext &C) const {
  RetainSummaryManager &Summaries = getSummaryManager(C);

  // Leave null if no receiver.
  QualType ReceiverType;
  if (const auto *MC = dyn_cast<ObjCMethodCall>(&Call)) {
    if (MC->isInstanceMessage()) {
      SVal ReceiverV = MC->getReceiverSVal();
      if (SymbolRef Sym = ReceiverV.getAsLocSymbol())
        if (const RefVal *T = getRefBinding(C.getState(), Sym))
          ReceiverType = T->getType();
    }
  }

  const RetainSummary *Summ = Summaries.getSummary(Call, ReceiverType);

  if (C.wasInlined) {
    processSummaryOfInlined(*Summ, Call, C);
    return;
  }
  checkSummary(*Summ, Call, C);
}

/// GetReturnType - Used to get the return type of a message expression or
///  function call with the intention of affixing that type to a tracked symbol.
///  While the return type can be queried directly from RetEx, when
///  invoking class methods we augment to the return type to be that of
///  a pointer to the class (as opposed it just being id).
// FIXME: We may be able to do this with related result types instead.
// This function is probably overestimating.
static QualType GetReturnType(const Expr *RetE, ASTContext &Ctx) {
  QualType RetTy = RetE->getType();
  // If RetE is not a message expression just return its type.
  // If RetE is a message expression, return its types if it is something
  /// more specific than id.
  if (const ObjCMessageExpr *ME = dyn_cast<ObjCMessageExpr>(RetE))
    if (const ObjCObjectPointerType *PT = RetTy->getAs<ObjCObjectPointerType>())
      if (PT->isObjCQualifiedIdType() || PT->isObjCIdType() ||
          PT->isObjCClassType()) {
        // At this point we know the return type of the message expression is
        // id, id<...>, or Class. If we have an ObjCInterfaceDecl, we know this
        // is a call to a class method whose type we can resolve.  In such
        // cases, promote the return type to XXX* (where XXX is the class).
        const ObjCInterfaceDecl *D = ME->getReceiverInterface();
        return !D ? RetTy :
                    Ctx.getObjCObjectPointerType(Ctx.getObjCInterfaceType(D));
      }

  return RetTy;
}

static Optional<RefVal> refValFromRetEffect(RetEffect RE,
                                            QualType ResultTy) {
  if (RE.isOwned()) {
    return RefVal::makeOwned(RE.getObjKind(), ResultTy);
  } else if (RE.notOwned()) {
    return RefVal::makeNotOwned(RE.getObjKind(), ResultTy);
  }

  return None;
}

// We don't always get the exact modeling of the function with regards to the
// retain count checker even when the function is inlined. For example, we need
// to stop tracking the symbols which were marked with StopTrackingHard.
void RetainCountChecker::processSummaryOfInlined(const RetainSummary &Summ,
                                                 const CallEvent &CallOrMsg,
                                                 CheckerContext &C) const {
  ProgramStateRef state = C.getState();

  // Evaluate the effect of the arguments.
  for (unsigned idx = 0, e = CallOrMsg.getNumArgs(); idx != e; ++idx) {
    if (Summ.getArg(idx) == StopTrackingHard) {
      SVal V = CallOrMsg.getArgSVal(idx);
      if (SymbolRef Sym = V.getAsLocSymbol()) {
        state = removeRefBinding(state, Sym);
      }
    }
  }

  // Evaluate the effect on the message receiver.
  if (const auto *MsgInvocation = dyn_cast<ObjCMethodCall>(&CallOrMsg)) {
    if (SymbolRef Sym = MsgInvocation->getReceiverSVal().getAsLocSymbol()) {
      if (Summ.getReceiverEffect() == StopTrackingHard) {
        state = removeRefBinding(state, Sym);
      }
    }
  }

  // Consult the summary for the return value.
  RetEffect RE = Summ.getRetEffect();

  if (SymbolRef Sym = CallOrMsg.getReturnValue().getAsSymbol()) {
    if (const auto *MCall = dyn_cast<CXXMemberCall>(&CallOrMsg)) {
      if (Optional<RefVal> updatedRefVal =
              refValFromRetEffect(RE, MCall->getResultType())) {
        state = setRefBinding(state, Sym, *updatedRefVal);
      }
    }

    if (RE.getKind() == RetEffect::NoRetHard)
      state = removeRefBinding(state, Sym);
  }

  C.addTransition(state);
}

static ProgramStateRef updateOutParameter(ProgramStateRef State,
                                          SVal ArgVal,
                                          ArgEffect Effect) {
  auto *ArgRegion = dyn_cast_or_null<TypedValueRegion>(ArgVal.getAsRegion());
  if (!ArgRegion)
    return State;

  QualType PointeeTy = ArgRegion->getValueType();
  if (!coreFoundation::isCFObjectRef(PointeeTy))
    return State;

  SVal PointeeVal = State->getSVal(ArgRegion);
  SymbolRef Pointee = PointeeVal.getAsLocSymbol();
  if (!Pointee)
    return State;

  switch (Effect) {
  case UnretainedOutParameter:
    State = setRefBinding(State, Pointee,
                          RefVal::makeNotOwned(RetEffect::CF, PointeeTy));
    break;
  case RetainedOutParameter:
    // Do nothing. Retained out parameters will either point to a +1 reference
    // or NULL, but the way you check for failure differs depending on the API.
    // Consequently, we don't have a good way to track them yet.
    break;

  default:
    llvm_unreachable("only for out parameters");
  }

  return State;
}

void RetainCountChecker::checkSummary(const RetainSummary &Summ,
                                      const CallEvent &CallOrMsg,
                                      CheckerContext &C) const {
  ProgramStateRef state = C.getState();

  // Evaluate the effect of the arguments.
  RefVal::Kind hasErr = (RefVal::Kind) 0;
  SourceRange ErrorRange;
  SymbolRef ErrorSym = nullptr;

  for (unsigned idx = 0, e = CallOrMsg.getNumArgs(); idx != e; ++idx) {
    SVal V = CallOrMsg.getArgSVal(idx);

    ArgEffect Effect = Summ.getArg(idx);
    if (Effect == RetainedOutParameter || Effect == UnretainedOutParameter) {
      state = updateOutParameter(state, V, Effect);
    } else if (SymbolRef Sym = V.getAsLocSymbol()) {
      if (const RefVal *T = getRefBinding(state, Sym)) {
        state = updateSymbol(state, Sym, *T, Effect, hasErr, C);
        if (hasErr) {
          ErrorRange = CallOrMsg.getArgSourceRange(idx);
          ErrorSym = Sym;
          break;
        }
      }
    }
  }

  // Evaluate the effect on the message receiver / `this` argument.
  bool ReceiverIsTracked = false;
  if (!hasErr) {
    if (const auto *MsgInvocation = dyn_cast<ObjCMethodCall>(&CallOrMsg)) {
      if (SymbolRef Sym = MsgInvocation->getReceiverSVal().getAsLocSymbol()) {
        if (const RefVal *T = getRefBinding(state, Sym)) {
          ReceiverIsTracked = true;
          state = updateSymbol(state, Sym, *T, Summ.getReceiverEffect(),
                                 hasErr, C);
          if (hasErr) {
            ErrorRange = MsgInvocation->getOriginExpr()->getReceiverRange();
            ErrorSym = Sym;
          }
        }
      }
    } else if (const auto *MCall = dyn_cast<CXXMemberCall>(&CallOrMsg)) {
      if (SymbolRef Sym = MCall->getCXXThisVal().getAsLocSymbol()) {
        if (const RefVal *T = getRefBinding(state, Sym)) {
          state = updateSymbol(state, Sym, *T, Summ.getThisEffect(),
                               hasErr, C);
          if (hasErr) {
            ErrorRange = MCall->getOriginExpr()->getSourceRange();
            ErrorSym = Sym;
          }
        }
      }
    }
  }

  // Process any errors.
  if (hasErr) {
    processNonLeakError(state, ErrorRange, hasErr, ErrorSym, C);
    return;
  }

  // Consult the summary for the return value.
  RetEffect RE = Summ.getRetEffect();

  if (RE.getKind() == RetEffect::OwnedWhenTrackedReceiver) {
    if (ReceiverIsTracked)
      RE = getSummaryManager(C).getObjAllocRetEffect();
    else
      RE = RetEffect::MakeNoRet();
  }

  if (SymbolRef Sym = CallOrMsg.getReturnValue().getAsSymbol()) {
    QualType ResultTy = CallOrMsg.getResultType();
    if (RE.notOwned()) {
      const Expr *Ex = CallOrMsg.getOriginExpr();
      assert(Ex);
      ResultTy = GetReturnType(Ex, C.getASTContext());
    }
    if (Optional<RefVal> updatedRefVal = refValFromRetEffect(RE, ResultTy))
      state = setRefBinding(state, Sym, *updatedRefVal);
  }

  // This check is actually necessary; otherwise the statement builder thinks
  // we've hit a previously-found path.
  // Normally addTransition takes care of this, but we want the node pointer.
  ExplodedNode *NewNode;
  if (state == C.getState()) {
    NewNode = C.getPredecessor();
  } else {
    NewNode = C.addTransition(state);
  }

  // Annotate the node with summary we used.
  if (NewNode) {
    // FIXME: This is ugly. See checkEndAnalysis for why it's necessary.
    if (ShouldResetSummaryLog) {
      SummaryLog.clear();
      ShouldResetSummaryLog = false;
    }
    SummaryLog[NewNode] = &Summ;
  }
}

ProgramStateRef
RetainCountChecker::updateSymbol(ProgramStateRef state, SymbolRef sym,
                                 RefVal V, ArgEffect E, RefVal::Kind &hasErr,
                                 CheckerContext &C) const {
  bool IgnoreRetainMsg = (bool)C.getASTContext().getLangOpts().ObjCAutoRefCount;
  switch (E) {
  default:
    break;
  case IncRefMsg:
    E = IgnoreRetainMsg ? DoNothing : IncRef;
    break;
  case DecRefMsg:
    E = IgnoreRetainMsg ? DoNothing: DecRef;
    break;
  case DecRefMsgAndStopTrackingHard:
    E = IgnoreRetainMsg ? StopTracking : DecRefAndStopTrackingHard;
    break;
  case MakeCollectable:
    E = DoNothing;
  }

  // Handle all use-after-releases.
  if (V.getKind() == RefVal::Released) {
    V = V ^ RefVal::ErrorUseAfterRelease;
    hasErr = V.getKind();
    return setRefBinding(state, sym, V);
  }

  switch (E) {
    case DecRefMsg:
    case IncRefMsg:
    case MakeCollectable:
    case DecRefMsgAndStopTrackingHard:
      llvm_unreachable("DecRefMsg/IncRefMsg/MakeCollectable already converted");

    case UnretainedOutParameter:
    case RetainedOutParameter:
      llvm_unreachable("Applies to pointer-to-pointer parameters, which should "
                       "not have ref state.");

    case Dealloc:
      switch (V.getKind()) {
        default:
          llvm_unreachable("Invalid RefVal state for an explicit dealloc.");
        case RefVal::Owned:
          // The object immediately transitions to the released state.
          V = V ^ RefVal::Released;
          V.clearCounts();
          return setRefBinding(state, sym, V);
        case RefVal::NotOwned:
          V = V ^ RefVal::ErrorDeallocNotOwned;
          hasErr = V.getKind();
          break;
      }
      break;

    case MayEscape:
      if (V.getKind() == RefVal::Owned) {
        V = V ^ RefVal::NotOwned;
        break;
      }

      // Fall-through.

    case DoNothing:
      return state;

    case Autorelease:
      // Update the autorelease counts.
      V = V.autorelease();
      break;

    case StopTracking:
    case StopTrackingHard:
      return removeRefBinding(state, sym);

    case IncRef:
      switch (V.getKind()) {
        default:
          llvm_unreachable("Invalid RefVal state for a retain.");
        case RefVal::Owned:
        case RefVal::NotOwned:
          V = V + 1;
          break;
      }
      break;

    case DecRef:
    case DecRefBridgedTransferred:
    case DecRefAndStopTrackingHard:
      switch (V.getKind()) {
        default:
          // case 'RefVal::Released' handled above.
          llvm_unreachable("Invalid RefVal state for a release.");

        case RefVal::Owned:
          assert(V.getCount() > 0);
          if (V.getCount() == 1) {
            if (E == DecRefBridgedTransferred ||
                V.getIvarAccessHistory() ==
                  RefVal::IvarAccessHistory::AccessedDirectly)
              V = V ^ RefVal::NotOwned;
            else
              V = V ^ RefVal::Released;
          } else if (E == DecRefAndStopTrackingHard) {
            return removeRefBinding(state, sym);
          }

          V = V - 1;
          break;

        case RefVal::NotOwned:
          if (V.getCount() > 0) {
            if (E == DecRefAndStopTrackingHard)
              return removeRefBinding(state, sym);
            V = V - 1;
          } else if (V.getIvarAccessHistory() ==
                       RefVal::IvarAccessHistory::AccessedDirectly) {
            // Assume that the instance variable was holding on the object at
            // +1, and we just didn't know.
            if (E == DecRefAndStopTrackingHard)
              return removeRefBinding(state, sym);
            V = V.releaseViaIvar() ^ RefVal::Released;
          } else {
            V = V ^ RefVal::ErrorReleaseNotOwned;
            hasErr = V.getKind();
          }
          break;
      }
      break;
  }
  return setRefBinding(state, sym, V);
}

void RetainCountChecker::processNonLeakError(ProgramStateRef St,
                                             SourceRange ErrorRange,
                                             RefVal::Kind ErrorKind,
                                             SymbolRef Sym,
                                             CheckerContext &C) const {
  // HACK: Ignore retain-count issues on values accessed through ivars,
  // because of cases like this:
  //   [_contentView retain];
  //   [_contentView removeFromSuperview];
  //   [self addSubview:_contentView]; // invalidates 'self'
  //   [_contentView release];
  if (const RefVal *RV = getRefBinding(St, Sym))
    if (RV->getIvarAccessHistory() != RefVal::IvarAccessHistory::None)
      return;

  ExplodedNode *N = C.generateErrorNode(St);
  if (!N)
    return;

  CFRefBug *BT;
  switch (ErrorKind) {
    default:
      llvm_unreachable("Unhandled error.");
    case RefVal::ErrorUseAfterRelease:
      if (!useAfterRelease)
        useAfterRelease.reset(new UseAfterRelease(this));
      BT = useAfterRelease.get();
      break;
    case RefVal::ErrorReleaseNotOwned:
      if (!releaseNotOwned)
        releaseNotOwned.reset(new BadRelease(this));
      BT = releaseNotOwned.get();
      break;
    case RefVal::ErrorDeallocNotOwned:
      if (!deallocNotOwned)
        deallocNotOwned.reset(new DeallocNotOwned(this));
      BT = deallocNotOwned.get();
      break;
  }

  assert(BT);
  auto report = llvm::make_unique<CFRefReport>(
      *BT, C.getASTContext().getLangOpts(), SummaryLog, N, Sym);
  report->addRange(ErrorRange);
  C.emitReport(std::move(report));
}

//===----------------------------------------------------------------------===//
// Handle the return values of retain-count-related functions.
//===----------------------------------------------------------------------===//

bool RetainCountChecker::evalCall(const CallExpr *CE, CheckerContext &C) const {
  // Get the callee. We're only interested in simple C functions.
  ProgramStateRef state = C.getState();
  const FunctionDecl *FD = C.getCalleeDecl(CE);
  if (!FD)
    return false;

  RetainSummaryManager &SmrMgr = getSummaryManager(C);
  QualType ResultTy = CE->getCallReturnType(C.getASTContext());

  // See if the function has 'rc_ownership_trusted_implementation'
  // annotate attribute. If it does, we will not inline it.
  bool hasTrustedImplementationAnnotation = false;

  // See if it's one of the specific functions we know how to eval.
  if (!SmrMgr.canEval(CE, FD, hasTrustedImplementationAnnotation))
    return false;

  // Bind the return value.
  const LocationContext *LCtx = C.getLocationContext();
  SVal RetVal = state->getSVal(CE->getArg(0), LCtx);
  if (RetVal.isUnknown() ||
      (hasTrustedImplementationAnnotation && !ResultTy.isNull())) {
    // If the receiver is unknown or the function has
    // 'rc_ownership_trusted_implementation' annotate attribute, conjure a
    // return value.
    SValBuilder &SVB = C.getSValBuilder();
    RetVal = SVB.conjureSymbolVal(nullptr, CE, LCtx, ResultTy, C.blockCount());
  }
  state = state->BindExpr(CE, LCtx, RetVal, false);

  // FIXME: This should not be necessary, but otherwise the argument seems to be
  // considered alive during the next statement.
  if (const MemRegion *ArgRegion = RetVal.getAsRegion()) {
    // Save the refcount status of the argument.
    SymbolRef Sym = RetVal.getAsLocSymbol();
    const RefVal *Binding = nullptr;
    if (Sym)
      Binding = getRefBinding(state, Sym);

    // Invalidate the argument region.
    state = state->invalidateRegions(
        ArgRegion, CE, C.blockCount(), LCtx,
        /*CausesPointerEscape*/ hasTrustedImplementationAnnotation);

    // Restore the refcount status of the argument.
    if (Binding)
      state = setRefBinding(state, Sym, *Binding);
  }

  C.addTransition(state);
  return true;
}

ExplodedNode * RetainCountChecker::processReturn(const ReturnStmt *S,
                                                 CheckerContext &C) const {
  ExplodedNode *Pred = C.getPredecessor();

  // Only adjust the reference count if this is the top-level call frame,
  // and not the result of inlining.  In the future, we should do
  // better checking even for inlined calls, and see if they match
  // with their expected semantics (e.g., the method should return a retained
  // object, etc.).
  if (!C.inTopFrame())
    return Pred;

  if (!S)
    return Pred;

  const Expr *RetE = S->getRetValue();
  if (!RetE)
    return Pred;

  ProgramStateRef state = C.getState();
  SymbolRef Sym =
    state->getSValAsScalarOrLoc(RetE, C.getLocationContext()).getAsLocSymbol();
  if (!Sym)
    return Pred;

  // Get the reference count binding (if any).
  const RefVal *T = getRefBinding(state, Sym);
  if (!T)
    return Pred;

  // Change the reference count.
  RefVal X = *T;

  switch (X.getKind()) {
    case RefVal::Owned: {
      unsigned cnt = X.getCount();
      assert(cnt > 0);
      X.setCount(cnt - 1);
      X = X ^ RefVal::ReturnedOwned;
      break;
    }

    case RefVal::NotOwned: {
      unsigned cnt = X.getCount();
      if (cnt) {
        X.setCount(cnt - 1);
        X = X ^ RefVal::ReturnedOwned;
      } else {
        X = X ^ RefVal::ReturnedNotOwned;
      }
      break;
    }

    default:
      return Pred;
  }

  // Update the binding.
  state = setRefBinding(state, Sym, X);
  Pred = C.addTransition(state);

  // At this point we have updated the state properly.
  // Everything after this is merely checking to see if the return value has
  // been over- or under-retained.

  // Did we cache out?
  if (!Pred)
    return nullptr;

  // Update the autorelease counts.
  static CheckerProgramPointTag AutoreleaseTag(this, "Autorelease");
  state = handleAutoreleaseCounts(state, Pred, &AutoreleaseTag, C, Sym, X, S);

  // Have we generated a sink node?
  if (!state)
    return nullptr;

  // Get the updated binding.
  T = getRefBinding(state, Sym);
  assert(T);
  X = *T;

  // Consult the summary of the enclosing method.
  RetainSummaryManager &Summaries = getSummaryManager(C);
  const Decl *CD = &Pred->getCodeDecl();
  RetEffect RE = RetEffect::MakeNoRet();

  // FIXME: What is the convention for blocks? Is there one?
  if (const ObjCMethodDecl *MD = dyn_cast<ObjCMethodDecl>(CD)) {
    const RetainSummary *Summ = Summaries.getMethodSummary(MD);
    RE = Summ->getRetEffect();
  } else if (const FunctionDecl *FD = dyn_cast<FunctionDecl>(CD)) {
    if (!isa<CXXMethodDecl>(FD)) {
      const RetainSummary *Summ = Summaries.getFunctionSummary(FD);
      RE = Summ->getRetEffect();
    }
  }

  return checkReturnWithRetEffect(S, C, Pred, RE, X, Sym, state);
}

ExplodedNode * RetainCountChecker::checkReturnWithRetEffect(const ReturnStmt *S,
                                                  CheckerContext &C,
                                                  ExplodedNode *Pred,
                                                  RetEffect RE, RefVal X,
                                                  SymbolRef Sym,
                                                  ProgramStateRef state) const {
  // HACK: Ignore retain-count issues on values accessed through ivars,
  // because of cases like this:
  //   [_contentView retain];
  //   [_contentView removeFromSuperview];
  //   [self addSubview:_contentView]; // invalidates 'self'
  //   [_contentView release];
  if (X.getIvarAccessHistory() != RefVal::IvarAccessHistory::None)
    return Pred;

  // Any leaks or other errors?
  if (X.isReturnedOwned() && X.getCount() == 0) {
    if (RE.getKind() != RetEffect::NoRet) {
      if (!RE.isOwned()) {

        // The returning type is a CF, we expect the enclosing method should
        // return ownership.
        X = X ^ RefVal::ErrorLeakReturned;

        // Generate an error node.
        state = setRefBinding(state, Sym, X);

        static CheckerProgramPointTag ReturnOwnLeakTag(this, "ReturnsOwnLeak");
        ExplodedNode *N = C.addTransition(state, Pred, &ReturnOwnLeakTag);
        if (N) {
          const LangOptions &LOpts = C.getASTContext().getLangOpts();
          auto R = llvm::make_unique<CFRefLeakReport>(
              *getLeakAtReturnBug(LOpts), LOpts, SummaryLog, N, Sym, C,
              IncludeAllocationLine);
          C.emitReport(std::move(R));
        }
        return N;
      }
    }
  } else if (X.isReturnedNotOwned()) {
    if (RE.isOwned()) {
      if (X.getIvarAccessHistory() ==
            RefVal::IvarAccessHistory::AccessedDirectly) {
        // Assume the method was trying to transfer a +1 reference from a
        // strong ivar to the caller.
        state = setRefBinding(state, Sym,
                              X.releaseViaIvar() ^ RefVal::ReturnedOwned);
      } else {
        // Trying to return a not owned object to a caller expecting an
        // owned object.
        state = setRefBinding(state, Sym, X ^ RefVal::ErrorReturnedNotOwned);

        static CheckerProgramPointTag
            ReturnNotOwnedTag(this, "ReturnNotOwnedForOwned");

        ExplodedNode *N = C.addTransition(state, Pred, &ReturnNotOwnedTag);
        if (N) {
          if (!returnNotOwnedForOwned)
            returnNotOwnedForOwned.reset(new ReturnedNotOwnedForOwned(this));

          auto R = llvm::make_unique<CFRefReport>(
              *returnNotOwnedForOwned, C.getASTContext().getLangOpts(),
              SummaryLog, N, Sym);
          C.emitReport(std::move(R));
        }
        return N;
      }
    }
  }
  return Pred;
}

//===----------------------------------------------------------------------===//
// Check various ways a symbol can be invalidated.
//===----------------------------------------------------------------------===//

void RetainCountChecker::checkBind(SVal loc, SVal val, const Stmt *S,
                                   CheckerContext &C) const {
  // Are we storing to something that causes the value to "escape"?
  bool escapes = true;

  // A value escapes in three possible cases (this may change):
  //
  // (1) we are binding to something that is not a memory region.
  // (2) we are binding to a memregion that does not have stack storage
  // (3) we are binding to a memregion with stack storage that the store
  //     does not understand.
  ProgramStateRef state = C.getState();

  if (auto regionLoc = loc.getAs<loc::MemRegionVal>()) {
    escapes = !regionLoc->getRegion()->hasStackStorage();

    if (!escapes) {
      // To test (3), generate a new state with the binding added.  If it is
      // the same state, then it escapes (since the store cannot represent
      // the binding).
      // Do this only if we know that the store is not supposed to generate the
      // same state.
      SVal StoredVal = state->getSVal(regionLoc->getRegion());
      if (StoredVal != val)
        escapes = (state == (state->bindLoc(*regionLoc, val, C.getLocationContext())));
    }
    if (!escapes) {
      // Case 4: We do not currently model what happens when a symbol is
      // assigned to a struct field, so be conservative here and let the symbol
      // go. TODO: This could definitely be improved upon.
      escapes = !isa<VarRegion>(regionLoc->getRegion());
    }
  }

  // If we are storing the value into an auto function scope variable annotated
  // with (__attribute__((cleanup))), stop tracking the value to avoid leak
  // false positives.
  if (const auto *LVR = dyn_cast_or_null<VarRegion>(loc.getAsRegion())) {
    const VarDecl *VD = LVR->getDecl();
    if (VD->hasAttr<CleanupAttr>()) {
      escapes = true;
    }
  }

  // If our store can represent the binding and we aren't storing to something
  // that doesn't have local storage then just return and have the simulation
  // state continue as is.
  if (!escapes)
      return;

  // Otherwise, find all symbols referenced by 'val' that we are tracking
  // and stop tracking them.
  state = state->scanReachableSymbols<StopTrackingCallback>(val).getState();
  C.addTransition(state);
}

ProgramStateRef RetainCountChecker::evalAssume(ProgramStateRef state,
                                               SVal Cond,
                                               bool Assumption) const {
  // FIXME: We may add to the interface of evalAssume the list of symbols
  //  whose assumptions have changed.  For now we just iterate through the
  //  bindings and check if any of the tracked symbols are NULL.  This isn't
  //  too bad since the number of symbols we will track in practice are
  //  probably small and evalAssume is only called at branches and a few
  //  other places.
  RefBindingsTy B = state->get<RefBindings>();

  if (B.isEmpty())
    return state;

  bool changed = false;
  RefBindingsTy::Factory &RefBFactory = state->get_context<RefBindings>();

  for (RefBindingsTy::iterator I = B.begin(), E = B.end(); I != E; ++I) {
    // Check if the symbol is null stop tracking the symbol.
    ConstraintManager &CMgr = state->getConstraintManager();
    ConditionTruthVal AllocFailed = CMgr.isNull(state, I.getKey());
    if (AllocFailed.isConstrainedTrue()) {
      changed = true;
      B = RefBFactory.remove(B, I.getKey());
    }
  }

  if (changed)
    state = state->set<RefBindings>(B);

  return state;
}

ProgramStateRef
RetainCountChecker::checkRegionChanges(ProgramStateRef state,
                                       const InvalidatedSymbols *invalidated,
                                       ArrayRef<const MemRegion *> ExplicitRegions,
                                       ArrayRef<const MemRegion *> Regions,
                                       const LocationContext *LCtx,
                                       const CallEvent *Call) const {
  if (!invalidated)
    return state;

  llvm::SmallPtrSet<SymbolRef, 8> WhitelistedSymbols;
  for (ArrayRef<const MemRegion *>::iterator I = ExplicitRegions.begin(),
       E = ExplicitRegions.end(); I != E; ++I) {
    if (const SymbolicRegion *SR = (*I)->StripCasts()->getAs<SymbolicRegion>())
      WhitelistedSymbols.insert(SR->getSymbol());
  }

  for (InvalidatedSymbols::const_iterator I=invalidated->begin(),
       E = invalidated->end(); I!=E; ++I) {
    SymbolRef sym = *I;
    if (WhitelistedSymbols.count(sym))
      continue;
    // Remove any existing reference-count binding.
    state = removeRefBinding(state, sym);
  }
  return state;
}

ProgramStateRef
RetainCountChecker::handleAutoreleaseCounts(ProgramStateRef state,
                                            ExplodedNode *Pred,
                                            const ProgramPointTag *Tag,
                                            CheckerContext &Ctx,
                                            SymbolRef Sym,
                                            RefVal V,
                                            const ReturnStmt *S) const {
  unsigned ACnt = V.getAutoreleaseCount();

  // No autorelease counts?  Nothing to be done.
  if (!ACnt)
    return state;

  unsigned Cnt = V.getCount();

  // FIXME: Handle sending 'autorelease' to already released object.

  if (V.getKind() == RefVal::ReturnedOwned)
    ++Cnt;

  // If we would over-release here, but we know the value came from an ivar,
  // assume it was a strong ivar that's just been relinquished.
  if (ACnt > Cnt &&
      V.getIvarAccessHistory() == RefVal::IvarAccessHistory::AccessedDirectly) {
    V = V.releaseViaIvar();
    --ACnt;
  }

  if (ACnt <= Cnt) {
    if (ACnt == Cnt) {
      V.clearCounts();
      if (V.getKind() == RefVal::ReturnedOwned) {
        V = V ^ RefVal::ReturnedNotOwned;
      } else {
        V = V ^ RefVal::NotOwned;
      }
    } else {
      V.setCount(V.getCount() - ACnt);
      V.setAutoreleaseCount(0);
    }
    return setRefBinding(state, Sym, V);
  }

  // HACK: Ignore retain-count issues on values accessed through ivars,
  // because of cases like this:
  //   [_contentView retain];
  //   [_contentView removeFromSuperview];
  //   [self addSubview:_contentView]; // invalidates 'self'
  //   [_contentView release];
  if (V.getIvarAccessHistory() != RefVal::IvarAccessHistory::None)
    return state;

  // Woah!  More autorelease counts then retain counts left.
  // Emit hard error.
  V = V ^ RefVal::ErrorOverAutorelease;
  state = setRefBinding(state, Sym, V);

  ExplodedNode *N = Ctx.generateSink(state, Pred, Tag);
  if (N) {
    SmallString<128> sbuf;
    llvm::raw_svector_ostream os(sbuf);
    os << "Object was autoreleased ";
    if (V.getAutoreleaseCount() > 1)
      os << V.getAutoreleaseCount() << " times but the object ";
    else
      os << "but ";
    os << "has a +" << V.getCount() << " retain count";

    if (!overAutorelease)
      overAutorelease.reset(new OverAutorelease(this));

    const LangOptions &LOpts = Ctx.getASTContext().getLangOpts();
    auto R = llvm::make_unique<CFRefReport>(*overAutorelease, LOpts, SummaryLog,
                                            N, Sym, os.str());
    Ctx.emitReport(std::move(R));
  }

  return nullptr;
}

ProgramStateRef
RetainCountChecker::handleSymbolDeath(ProgramStateRef state,
                                      SymbolRef sid, RefVal V,
                                    SmallVectorImpl<SymbolRef> &Leaked) const {
  bool hasLeak;

  // HACK: Ignore retain-count issues on values accessed through ivars,
  // because of cases like this:
  //   [_contentView retain];
  //   [_contentView removeFromSuperview];
  //   [self addSubview:_contentView]; // invalidates 'self'
  //   [_contentView release];
  if (V.getIvarAccessHistory() != RefVal::IvarAccessHistory::None)
    hasLeak = false;
  else if (V.isOwned())
    hasLeak = true;
  else if (V.isNotOwned() || V.isReturnedOwned())
    hasLeak = (V.getCount() > 0);
  else
    hasLeak = false;

  if (!hasLeak)
    return removeRefBinding(state, sid);

  Leaked.push_back(sid);
  return setRefBinding(state, sid, V ^ RefVal::ErrorLeak);
}

ExplodedNode *
RetainCountChecker::processLeaks(ProgramStateRef state,
                                 SmallVectorImpl<SymbolRef> &Leaked,
                                 CheckerContext &Ctx,
                                 ExplodedNode *Pred) const {
  // Generate an intermediate node representing the leak point.
  ExplodedNode *N = Ctx.addTransition(state, Pred);

  if (N) {
    for (SmallVectorImpl<SymbolRef>::iterator
         I = Leaked.begin(), E = Leaked.end(); I != E; ++I) {

      const LangOptions &LOpts = Ctx.getASTContext().getLangOpts();
      CFRefBug *BT = Pred ? getLeakWithinFunctionBug(LOpts)
                          : getLeakAtReturnBug(LOpts);
      assert(BT && "BugType not initialized.");

      Ctx.emitReport(llvm::make_unique<CFRefLeakReport>(
          *BT, LOpts, SummaryLog, N, *I, Ctx, IncludeAllocationLine));
    }
  }

  return N;
}

static bool isISLObjectRef(QualType Ty) {
  return StringRef(Ty.getAsString()).startswith("isl_");
}

void RetainCountChecker::checkBeginFunction(CheckerContext &Ctx) const {
  if (!Ctx.inTopFrame())
    return;

  RetainSummaryManager &SmrMgr = getSummaryManager(Ctx);
  const LocationContext *LCtx = Ctx.getLocationContext();
  const FunctionDecl *FD = dyn_cast<FunctionDecl>(LCtx->getDecl());

  if (!FD || SmrMgr.isTrustedReferenceCountImplementation(FD))
    return;

  ProgramStateRef state = Ctx.getState();
  const RetainSummary *FunctionSummary = SmrMgr.getFunctionSummary(FD);
  ArgEffects CalleeSideArgEffects = FunctionSummary->getArgEffects();

  for (unsigned idx = 0, e = FD->getNumParams(); idx != e; ++idx) {
    const ParmVarDecl *Param = FD->getParamDecl(idx);
    SymbolRef Sym = state->getSVal(state->getRegion(Param, LCtx)).getAsSymbol();

    QualType Ty = Param->getType();
    const ArgEffect *AE = CalleeSideArgEffects.lookup(idx);
    if (AE && *AE == DecRef && isISLObjectRef(Ty)) {
      state = setRefBinding(
          state, Sym, RefVal::makeOwned(RetEffect::ObjKind::Generalized, Ty));
    } else if (isISLObjectRef(Ty)) {
      state = setRefBinding(
          state, Sym,
          RefVal::makeNotOwned(RetEffect::ObjKind::Generalized, Ty));
    }
  }

  Ctx.addTransition(state);
}

void RetainCountChecker::checkEndFunction(const ReturnStmt *RS,
                                          CheckerContext &Ctx) const {
  ExplodedNode *Pred = processReturn(RS, Ctx);

  // Created state cached out.
  if (!Pred) {
    return;
  }

  ProgramStateRef state = Pred->getState();
  RefBindingsTy B = state->get<RefBindings>();

  // Don't process anything within synthesized bodies.
  const LocationContext *LCtx = Pred->getLocationContext();
  if (LCtx->getAnalysisDeclContext()->isBodyAutosynthesized()) {
    assert(!LCtx->inTopFrame());
    return;
  }

  for (RefBindingsTy::iterator I = B.begin(), E = B.end(); I != E; ++I) {
    state = handleAutoreleaseCounts(state, Pred, /*Tag=*/nullptr, Ctx,
                                    I->first, I->second);
    if (!state)
      return;
  }

  // If the current LocationContext has a parent, don't check for leaks.
  // We will do that later.
  // FIXME: we should instead check for imbalances of the retain/releases,
  // and suggest annotations.
  if (LCtx->getParent())
    return;

  B = state->get<RefBindings>();
  SmallVector<SymbolRef, 10> Leaked;

  for (RefBindingsTy::iterator I = B.begin(), E = B.end(); I != E; ++I)
    state = handleSymbolDeath(state, I->first, I->second, Leaked);

  processLeaks(state, Leaked, Ctx, Pred);
}

const ProgramPointTag *
RetainCountChecker::getDeadSymbolTag(SymbolRef sym) const {
  const CheckerProgramPointTag *&tag = DeadSymbolTags[sym];
  if (!tag) {
    SmallString<64> buf;
    llvm::raw_svector_ostream out(buf);
    out << "Dead Symbol : ";
    sym->dumpToStream(out);
    tag = new CheckerProgramPointTag(this, out.str());
  }
  return tag;
}

void RetainCountChecker::checkDeadSymbols(SymbolReaper &SymReaper,
                                          CheckerContext &C) const {
  ExplodedNode *Pred = C.getPredecessor();

  ProgramStateRef state = C.getState();
  RefBindingsTy B = state->get<RefBindings>();
  SmallVector<SymbolRef, 10> Leaked;

  // Update counts from autorelease pools
  for (SymbolReaper::dead_iterator I = SymReaper.dead_begin(),
       E = SymReaper.dead_end(); I != E; ++I) {
    SymbolRef Sym = *I;
    if (const RefVal *T = B.lookup(Sym)){
      // Use the symbol as the tag.
      // FIXME: This might not be as unique as we would like.
      const ProgramPointTag *Tag = getDeadSymbolTag(Sym);
      state = handleAutoreleaseCounts(state, Pred, Tag, C, Sym, *T);
      if (!state)
        return;

      // Fetch the new reference count from the state, and use it to handle
      // this symbol.
      state = handleSymbolDeath(state, *I, *getRefBinding(state, Sym), Leaked);
    }
  }

  if (Leaked.empty()) {
    C.addTransition(state);
    return;
  }

  Pred = processLeaks(state, Leaked, C, Pred);

  // Did we cache out?
  if (!Pred)
    return;

  // Now generate a new node that nukes the old bindings.
  // The only bindings left at this point are the leaked symbols.
  RefBindingsTy::Factory &F = state->get_context<RefBindings>();
  B = state->get<RefBindings>();

  for (SmallVectorImpl<SymbolRef>::iterator I = Leaked.begin(),
                                            E = Leaked.end();
       I != E; ++I)
    B = F.remove(B, *I);

  state = state->set<RefBindings>(B);
  C.addTransition(state, Pred);
}

void RetainCountChecker::printState(raw_ostream &Out, ProgramStateRef State,
                                    const char *NL, const char *Sep) const {

  RefBindingsTy B = State->get<RefBindings>();

  if (B.isEmpty())
    return;

  Out << Sep << NL;

  for (RefBindingsTy::iterator I = B.begin(), E = B.end(); I != E; ++I) {
    Out << I->first << " : ";
    I->second.print(Out);
    Out << NL;
  }
}

//===----------------------------------------------------------------------===//
// Checker registration.
//===----------------------------------------------------------------------===//

void ento::registerRetainCountChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<RetainCountChecker>(Mgr.getAnalyzerOptions());
}

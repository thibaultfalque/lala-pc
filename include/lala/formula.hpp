// Copyright 2021 Pierre Talbot

#ifndef LALA_PC_FORMULA_HPP
#define LALA_PC_FORMULA_HPP

#include "lala/universes/primitive_upset.hpp"

namespace lala {
namespace pc {

template <class AD, class Allocator>
class Formula;

template <class AD, class Allocator>
class AbstractElement {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;

  using tell_type = typename A::template tell_type<allocator_type>;
  using ask_type = typename A::template ask_type<allocator_type>;
  using this_type = AbstractElement<A, allocator_type>;

  template <class A2, class Alloc>
  friend class AbstractElement;

private:
  tell_type tellv;
  tell_type not_tellv;
  ask_type askv;
  ask_type not_askv;

public:
  CUDA AbstractElement() {}

  CUDA AbstractElement(
    tell_type&& tellv, tell_type&& not_tellv,
    ask_type&& askv, ask_type&& not_askv)
   : tellv(std::move(tellv)), not_tellv(std::move(not_tellv)), askv(std::move(askv)), not_askv(std::move(not_askv)) {}

  AbstractElement(this_type&& other) = default;
  this_type& operator=(this_type&& other) = default;

  template <class A2, class Alloc2>
  CUDA AbstractElement(const AbstractElement<A2, Alloc2>& other, const allocator_type& alloc)
   : tellv(other.tellv, alloc), not_tellv(other.not_tellv, alloc)
   , askv(other.askv, alloc), not_askv(other.not_askv, alloc) {}

public:
  CUDA local::BInc ask(const A& a) const {
    return a.ask(askv);
  }

  CUDA local::BInc nask(const A& a) const {
    return a.ask(not_askv);
  }

  template <class Mem>
  CUDA void preprocess(A&, BInc<Mem>&) {}

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    a.tell(tellv, has_changed);
  }

  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    a.tell(not_tellv, has_changed);
  }

  CUDA NI void print(const A& a) const {
    printf("<abstract element (%d)>", a.aty());
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType) const {
    return a.deinterpret(tellv, env);
  }

  CUDA size_t length() const { return 1; }
};

/** A Boolean variable defined on a universe of discourse supporting `0` for false and `1` for true. */
template<class AD, bool neg>
class VariableLiteral {
public:
  using A = AD;
  using U = typename A::local_universe;

  template <class A2, bool neg2>
  friend class VariableLiteral;

private:
  AVar avar;

public:
  VariableLiteral() = default;
  CUDA VariableLiteral(AVar avar): avar(avar) {}

  template <class A2, class Alloc>
  CUDA VariableLiteral(const VariableLiteral<A2, neg>& other, const Alloc&): avar(other.avar) {}

private:
  template<bool negate>
  CUDA local::BInc ask_impl(const A& a) const {
    if constexpr(negate) {
      return a.project(avar) >= U::eq_zero();
    }
    else {
      return !(a.project(avar) <= U::eq_zero());
    }
  }

  template <bool negate, class Mem>
  CUDA void refine_impl(A& a, BInc<Mem>& has_changed) const {
    if constexpr(negate) {
      a.tell(avar, U::eq_zero(), has_changed);
    }
    else {
      a.tell(avar, U::eq_one(), has_changed);
    }
  }

public:
  /** Given a variable `x` taking a value in a universe `U` denoted by \f$ a(x) \f$.
   *   - \f$ a \vDash x \f$ holds iff \f$ \lnot (a(x) \leq [\![x = 0]\!]_U) \f$.
   *   - \f$ a \vDash \lnot{x} \f$ holds iff \f$ a(x) \geq [\![x = 0]\!]_U \f$. */
  CUDA local::BInc ask(const A& a) const {
    return ask_impl<neg>(a);
  }

  /** Given a variable `x` taking a value in a universe `U` denoted by \f$ a(x) \f$.
   *   - \f$ a \nvDash x \f$ holds iff \f$ a(x) \geq [\![x = 0]\!]_U \f$.
   *   - \f$ a \nvDash \lnot{x} \f$ holds iff \f$ \lnot (a(x) \leq [\![x = 0]\!]_U) \f$. */
  CUDA local::BInc nask(const A& a) const {
    return ask_impl<!neg>(a);
  }

  template <class Mem>
  CUDA void preprocess(A&, BInc<Mem>&) {}

  /** Perform:
   *    * Positive literal: \f$ x = a(x) \sqcup [\![x = 1]\!]_U \f$
   *    * Negative literal: \f$ x = a(x) \sqcup [\![x = 0]\!]_U \f$. */
  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    refine_impl<neg>(a, has_changed);
  }

  /** Perform:
   *    * Positive literal: \f$ x = a(x) \sqcup [\![x = 0]\!]_U \f$
   *    * Negative literal: \f$ x = a(x) \sqcup [\![x = 1]\!]_U \f$. */
  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    refine_impl<!neg>(a, has_changed);
  }

  CUDA NI void print(const A& a) const {
    if constexpr(neg) { printf("not "); }
    printf("(%d,%d)", avar.aty(), avar.vid());
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A&, const Env& env, AType apc) const {
    using F = TFormula<typename Env::allocator_type>;
    auto f = F::make_lvar(avar.aty(), env.name_of(avar));
    if constexpr(neg) {
      f = F::make_unary(NOT, f, apc);
    }
    return f;
  }

  CUDA size_t length() const { return 1; }
};

template<class AD>
class False {
public:
  using A = AD;
  using U = typename A::local_universe;

  template <class A2>
  friend class False;

public:
  False() = default;

  template <class A2, class Alloc>
  CUDA False(const False<A2>&, const Alloc&) {}

  CUDA local::BInc ask(const A& a) const { return a.is_top(); }
  CUDA local::BInc nask(const A&) const { return true; }

  template <class Mem>
  CUDA void preprocess(A&, BInc<Mem>&) {}

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    if(!a.is_top()) {
      has_changed.tell_top();
      a.tell_top();
    }
  }

  template <class Mem>
  CUDA void nrefine(A&, BInc<Mem>&) const {}
  CUDA NI void print(const A& a) const { printf("false"); }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A&, const Env&, AType) const {
    return TFormula<typename Env::allocator_type>::make_false();
  }

  CUDA size_t length() const { return 1; }
};

template<class AD>
class True {
public:
  using A = AD;
  using U = typename A::local_universe;

  template <class A2>
  friend class True;

public:
  True() = default;

  template <class A2, class Alloc>
  CUDA True(const True<A2>&, const Alloc&) {}

  CUDA local::BInc ask(const A& a) const { return true; }
  CUDA local::BInc nask(const A& a) const { return a.is_top(); }
  template <class Mem> CUDA void preprocess(A&, BInc<Mem>&) {}
  template <class Mem> CUDA void refine(A&, BInc<Mem>&) const {}
  template <class Mem> CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    if(!a.is_top()) {
      has_changed.tell_top();
      a.tell_top();
    }
  }
  CUDA void print(const A& a) const { printf("true"); }

  template <class Env>
  CUDA TFormula<typename Env::allocator_type> deinterpret(const A&, const Env&, AType) const {
    return TFormula<typename Env::allocator_type>::make_true();
  }

  CUDA size_t length() const { return 1; }
};

/** A predicate of the form `t >= u` where `t` is a term, `u` an element of an abstract universe, and `>=` the lattice order of this abstract universe. */
template<class AD, class Allocator>
class PLatticeOrderPredicate {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = PLatticeOrderPredicate<A, allocator_type>;
  using sub_type = Term<A, allocator_type>;

  template <class A2, class Alloc2>
  friend class PLatticeOrderPredicate;

  sub_type left;
  U right;

  CUDA PLatticeOrderPredicate(sub_type&& left, U&& right): left(std::move(left)), right(std::move(right)) {}
  CUDA PLatticeOrderPredicate(this_type&& other): PLatticeOrderPredicate(std::move(other.left), std::move(other.right)) {}

  template <class A2, class Alloc2>
  CUDA PLatticeOrderPredicate(const PLatticeOrderPredicate<A2, Alloc2>& other, const allocator_type& alloc):
    left(other.left, alloc),
    right(other.right)
  {}

  CUDA local::BInc ask(const A& a) const {
    return left.project(a) >= right;
  }

  CUDA local::BInc nask(const A&) const { assert(false); return local::BInc::bot(); }
  template <class Mem> CUDA void nrefine(A&, BInc<Mem>&) const { assert(false); }
  template <class Mem> CUDA void tell(A&, const U&, BInc<Mem>&) const { assert(false); }
  CUDA U project(const A&) const { assert(false); return U::top(); }

  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    right.tell(left.project(a), has_changed);
  }

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    left.tell(a, right, has_changed);
  }

  CUDA NI void print(const A& a) const {
    left.print(a);
    printf(" >= ");
    right.print();
  }

private:
  template <class Alloc> using F = TFormula<Alloc>;
  template <class Alloc> using Seq = typename F<Alloc>::Sequence;

  template <class Alloc>
  CUDA Seq<Alloc> map_seq(const Seq<Alloc>& seq, const F<Alloc>& t) const {
    Seq<Alloc> seq2{seq.get_allocator()};
    for(int i = 0; i < seq.size(); ++i) {
      seq2.push_back(map_avar(seq[i], t));
    }
    return std::move(seq2);
  }

  template <class Alloc>
  CUDA F<Alloc> map_avar(const F<Alloc>& f, const F<Alloc>& t) const {
    switch(f.index()) {
      case F<Alloc>::V: return t;
      case F<Alloc>::Seq: return F<Alloc>::make_nary(f.sig(), map_seq(f.seq(), t), f.type());
      case F<Alloc>::ESeq: return F<Alloc>::make_nary(f.esig(), map_seq(f.eseq(), t), f.type());
      default: return f;
    }
  }

public:
  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    // We deinterpret the constant with a placeholder variable that is then replaced by the interpretation of the left term.
    auto uf = right.deinterpret(AVar{}, env);
    auto tf = left.deinterpret(a, env, apc);
    return map_avar(uf, tf);
  }

  CUDA size_t length() const { return left.length() + 2; }
};

template<class AD, class Allocator>
class NLatticeOrderPredicate :
  public PLatticeOrderPredicate<AD, Allocator>
{
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = NLatticeOrderPredicate<A, allocator_type>;
  using sub_type = Term<A, allocator_type>;
  using formula_ptr = battery::unique_ptr<Formula<A, allocator_type>, allocator_type>;

  template <class A2, class Alloc2>
  friend class NLatticeOrderPredicate;

private:
  using base_type = PLatticeOrderPredicate<AD, Allocator>;
  formula_ptr not_f;

public:
  CUDA NLatticeOrderPredicate(sub_type&& left, U&& right, formula_ptr&& not_f)
   : base_type(std::move(left), std::move(right)), not_f(std::move(not_f)) {}

  CUDA NLatticeOrderPredicate(this_type&& other)
   : NLatticeOrderPredicate(std::move(other.left), std::move(other.right), std::move(other.not_f)) {}

  template <class A2, class Alloc2>
  CUDA NLatticeOrderPredicate(const NLatticeOrderPredicate<A2, Alloc2>& other, const allocator_type& alloc)
   : base_type(other, alloc)
   , not_f(battery::allocate_unique<Formula<A, allocator_type>>(alloc, *other.not_f, alloc))
  {}

  CUDA local::BInc nask(const A& a) const {
    return not_f->ask(a);
  }

  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    not_f->refine(a, has_changed);
  }

  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    base_type::preprocess(a, has_changed);
    not_f->preprocess(a, has_changed);
  }
};

template<class AD, class Allocator>
class Conjunction {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = Conjunction<A, allocator_type>;
  using sub_type = Formula<A, allocator_type>;
  using sub_ptr = battery::unique_ptr<sub_type, allocator_type>;

  template <class A2, class Alloc2>
  friend class Conjunction;

private:
  sub_ptr f;
  sub_ptr g;

public:
  CUDA Conjunction(sub_ptr&& f, sub_ptr&& g): f(std::move(f)), g(std::move(g)) {}
  CUDA Conjunction(this_type&& other): Conjunction(std::move(other.f), std::move(other.g)) {}

  template <class A2, class Alloc2>
  CUDA Conjunction(const Conjunction<A2, Alloc2>& other, const allocator_type& alloc)
   : f(battery::allocate_unique<sub_type>(alloc, *other.f, alloc))
   , g(battery::allocate_unique<sub_type>(alloc, *other.g, alloc))
  {}

  CUDA local::BInc ask(const A& a) const {
    return f->ask(a) && g->ask(a);
  }

  CUDA local::BInc nask(const A& a) const {
    return f->nask(a) || g->nask(a);
  }

  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    f->preprocess(a, has_changed);
    g->preprocess(a, has_changed);
  }

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    f->refine(a, has_changed);
    g->refine(a, has_changed);
  }

  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    if(f->ask(a)) { g->nrefine(a, has_changed); }
    else if(g->ask(a)) { f->nrefine(a, has_changed); }
  }

  CUDA NI void print(const A& a) const {
    f->print(a);
    printf(" /\\ ");
    g->print(a);
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    auto left = f->deinterpret(a, env, apc);
    auto right = g->deinterpret(a, env, apc);
    if(left.is_true()) { return right; }
    else if(right.is_true()) { return left; }
    else if(left.is_false()) { return left; }
    else if(right.is_false()) { return right; }
    else {
      return TFormula<typename Env::allocator_type>::make_binary(left, AND, right, apc, env.get_allocator());
    }
  }

  CUDA size_t length() const { return 1 + f->length() + g->length(); }
};

template<class AD, class Allocator>
class Disjunction {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = Disjunction<A, allocator_type>;
  using sub_type = Formula<A, allocator_type>;
  using sub_ptr = battery::unique_ptr<sub_type, allocator_type>;

  template <class A2, class Alloc2>
  friend class Disjunction;

private:
  sub_ptr f;
  sub_ptr g;

public:
  CUDA Disjunction(sub_ptr&& f, sub_ptr&& g): f(std::move(f)), g(std::move(g)) {}
  CUDA Disjunction(this_type&& other): Disjunction(
    std::move(other.f), std::move(other.g)) {}

  template <class A2, class Alloc2>
  CUDA Disjunction(const Disjunction<A2, Alloc2>& other, const allocator_type& alloc)
   : f(battery::allocate_unique<sub_type>(alloc, *other.f, alloc))
   , g(battery::allocate_unique<sub_type>(alloc, *other.g, alloc))
  {}

  CUDA local::BInc ask(const A& a) const {
    return f->ask(a) || g->ask(a);
  }

  CUDA local::BInc nask(const A& a) const {
    return f->nask(a) && g->nask(a);
  }

  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    f->preprocess(a, has_changed);
    g->preprocess(a, has_changed);
  }

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    if(f->nask(a)) { g->refine(a, has_changed); }
    else if(g->nask(a)) { f->refine(a, has_changed); }
  }

  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    f->nrefine(a, has_changed);
    g->nrefine(a, has_changed);
  }

  CUDA NI void print(const A& a) const {
    f->print(a);
    printf(" \\/ ");
    g->print(a);
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    auto left = f->deinterpret(a, env, apc);
    auto right = g->deinterpret(a, env, apc);
    if(left.is_true()) { return left; }
    else if(right.is_true()) { return right; }
    else if(left.is_false()) { return right; }
    else if(right.is_false()) { return left; }
    else {
      return TFormula<typename Env::allocator_type>::make_binary(left, OR, right, apc, env.get_allocator());
    }
  }

  CUDA size_t length() const { return 1 + f->length() + g->length(); }
};

template<class AD, class Allocator>
class Biconditional {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = Biconditional<AD, Allocator>;
  using sub_type = Formula<A, allocator_type>;
  using sub_ptr = battery::unique_ptr<sub_type, allocator_type>;

  template <class A2, class Alloc2>
  friend class Biconditional;

private:
  sub_ptr f;
  sub_ptr g;

public:
  CUDA Biconditional(sub_ptr&& f, sub_ptr&& g): f(std::move(f)), g(std::move(g)) {}
  CUDA Biconditional(this_type&& other): Biconditional(
    std::move(other.f), std::move(other.g)) {}

  template <class A2, class Alloc2>
  CUDA Biconditional(const Biconditional<A2, Alloc2>& other, const allocator_type& alloc)
   : f(battery::allocate_unique<sub_type>(alloc, *other.f, alloc))
   , g(battery::allocate_unique<sub_type>(alloc, *other.g, alloc))
  {}

  CUDA local::BInc ask(const A& a) const {
    return
      (f->ask(a) && g->ask(a)) ||
      (f->nask(a) && g->nask(a));
  }

  // note that not(f <=> g) is equivalent to (f <=> not g)
  CUDA local::BInc nask(const A& a) const {
    return
      (f->ask(a) && g->nask(a)) ||
      (f->nask(a) && g->ask(a));
  }

  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    f->preprocess(a, has_changed);
    g->preprocess(a, has_changed);
  }

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    if(f->ask(a)) { g->refine(a, has_changed); }
    else if(f->nask(a)) { g->nrefine(a, has_changed); }
    else if(g->ask(a)) { f->refine(a, has_changed); }
    else if(g->nask(a)) { f->nrefine(a, has_changed); }
  }

  // note that not(f <=> g) is equivalent to (f <=> not g)
  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    if(f->ask(a)) { g->nrefine(a, has_changed); }
    else if(f->nask(a)) { g->refine(a, has_changed); }
    else if(g->ask(a)) { f->nrefine(a, has_changed); }
    else if(g->nask(a)) { f->refine(a, has_changed); }
  }

  CUDA NI void print(const A& a) const {
    f->print(a);
    printf(" <=> ");
    g->print(a);
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    return TFormula<typename Env::allocator_type>::make_binary(
      f->deinterpret(a, env, apc), EQUIV, g->deinterpret(a, env, apc), apc, env.get_allocator());
  }

  CUDA size_t length() const { return 1 + f->length() + g->length(); }
};

template<class AD, class Allocator>
class Implication {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = Implication<AD, Allocator>;
  using sub_type = Formula<A, allocator_type>;
  using sub_ptr = battery::unique_ptr<sub_type, allocator_type>;

  template <class A2, class Alloc2>
  friend class Implication;

private:
  sub_ptr f;
  sub_ptr g;

public:
  CUDA Implication(sub_ptr&& f, sub_ptr&& g): f(std::move(f)), g(std::move(g)) {}
  CUDA Implication(this_type&& other): Implication(
    std::move(other.f), std::move(other.g)) {}

  template <class A2, class Alloc2>
  CUDA Implication(const Implication<A2, Alloc2>& other, const allocator_type& alloc)
   : f(battery::allocate_unique<sub_type>(alloc, *other.f, alloc))
   , g(battery::allocate_unique<sub_type>(alloc, *other.g, alloc))
  {}

  // note that f => g is equivalent to (not f) or g.
  CUDA local::BInc ask(const A& a) const {
    return f->nask(a) || g->ask(a);
  }

  // note that not(f => g) is equivalent to f and (not g)
  CUDA local::BInc nask(const A& a) const {
    return f->ask(a) && g->nask(a);
  }

  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    f->preprocess(a, has_changed);
    g->preprocess(a, has_changed);
  }

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    if(f->ask(a)) { g->refine(a, has_changed); }
    else if(g->nask(a)) { f->nrefine(a, has_changed); }
  }

  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    if(f->ask(a)) { g->nrefine(a, has_changed); }
    else if(g->nask(a)) { f->refine(a, has_changed); }
  }

  CUDA NI void print(const A& a) const {
    f->print(a);
    printf(" => ");
    g->print(a);
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    return TFormula<typename Env::allocator_type>::make_binary(
      f->deinterpret(a, env, apc), IMPLY, g->deinterpret(a, env, apc), apc, env.get_allocator());
  }

  CUDA size_t length() const { return 1 + f->length() + g->length(); }
};

template<class AD, class Allocator>
class ExclusiveDisjunction {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = ExclusiveDisjunction<AD, Allocator>;
  using sub_type = Formula<A, allocator_type>;
  using sub_ptr = battery::unique_ptr<sub_type, allocator_type>;

  template <class A2, class Alloc2>
  friend class ExclusiveDisjunction;

private:
  sub_ptr f;
  sub_ptr g;

public:
  CUDA ExclusiveDisjunction(sub_ptr&& f, sub_ptr&& g): f(std::move(f)), g(std::move(g)) {}
  CUDA ExclusiveDisjunction(this_type&& other): ExclusiveDisjunction(
    std::move(other.f), std::move(other.g)) {}

  template <class A2, class Alloc2>
  CUDA ExclusiveDisjunction(const ExclusiveDisjunction<A2, Alloc2>& other, const allocator_type& alloc)
   : f(battery::allocate_unique<sub_type>(alloc, *other.f, alloc))
   , g(battery::allocate_unique<sub_type>(alloc, *other.g, alloc))
  {}

  CUDA local::BInc ask(const A& a) const {
    return
      (f->ask(a) && g->nask(a)) ||
      (f->nask(a) && g->ask(a));
  }

  CUDA local::BInc nask(const A& a) const {
    return
      (f->ask(a) && g->ask(a)) ||
      (f->ask(a) && g->nask(a));
  }

  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    f->preprocess(a, has_changed);
    g->preprocess(a, has_changed);
  }

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    if(f->ask(a)) { g->nrefine(a, has_changed); }
    else if(f->nask(a)) { g->refine(a, has_changed); }
    else if(g->ask(a)) { f->nrefine(a, has_changed); }
    else if(g->nask(a)) { f->refine(a, has_changed); }
  }

  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    if(f->ask(a)) { g->refine(a, has_changed); }
    else if(f->nask(a)) { g->nrefine(a, has_changed); }
    else if(g->ask(a)) { f->refine(a, has_changed); }
    else if(g->nask(a)) { f->nrefine(a, has_changed); }
  }

  CUDA NI void print(const A& a) const {
    f->print(a);
    printf(" xor ");
    g->print(a);
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    return TFormula<typename Env::allocator_type>::make_binary(
      f->deinterpret(a, env, apc), XOR, g->deinterpret(a, env, apc), apc, env.get_allocator());
  }

  CUDA size_t length() const { return 1 + f->length() + g->length(); }
};

template<class AD, class Allocator, bool neg = false>
class Equality {
public:
  using A = AD;
  using U = typename A::local_universe;
  using allocator_type = Allocator;
  using this_type = Equality<A, allocator_type, neg>;
  using sub_type = Term<A, allocator_type>;

  template <class A2, class Alloc2, bool neg2>
  friend class Equality;

private:
  using LB = typename U::LB::local_type;
  using UB = typename U::UB::local_type;

  sub_type left;
  sub_type right;

public:
  CUDA Equality(sub_type&& left, sub_type&& right): left(std::move(left)), right(std::move(right)) {}
  CUDA Equality(this_type&& other): Equality(std::move(other.left), std::move(other.right)) {}

  template <class A2, class Alloc2>
  CUDA Equality(const Equality<A2, Alloc2, neg>& other, const allocator_type& alloc):
    left(other.left, alloc),
    right(other.right, alloc)
  {}

private:
  template <bool negate>
  CUDA local::BInc ask_impl(const A& a) const {
    if constexpr(negate) {
      return join(left.project(a), right.project(a)).is_top();
    }
    else {
      auto l = left.project(a);
      auto r = right.project(a);
      return l == r && l.lb() == dual<LB>(l.ub());
    }
  }

  template <bool negate, class Mem>
  CUDA void refine_impl(A& a, BInc<Mem>& has_changed) const {
    if constexpr(negate) {
      auto l = left.project(a);
      auto r = right.project(a);
      if(l.lb() == dual<LB>(l.ub())) {
        if constexpr(U::complemented) {
          right.tell(a, l.complement(), has_changed);
        }
        else {
          right.tell(a, meet(
            U(r).tell_ub(UB::next(l.ub())),
            U(r).tell_lb(LB::next(l.lb()))), has_changed);
        }
      }
      if(r.lb() == dual<LB>(r.ub())) {
        if constexpr(U::complemented) {
          left.tell(a, r.complement(), has_changed);
        }
        else {
          left.tell(a, meet(
            U(l).tell_ub(UB::next(r.ub())),
            U(l).tell_lb(LB::next(r.lb()))), has_changed);
        }
      }
    }
    else {
      left.tell(a, right.project(a), has_changed);
      right.tell(a, left.project(a), has_changed);
    }
  }

public:
  CUDA local::BInc ask(const A& a) const {
    return ask_impl<neg>(a);
  }

  CUDA local::BInc nask(const A& a) const {
    return ask_impl<!neg>(a);
  }

  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    return refine_impl<neg>(a, has_changed);
  }

  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    return refine_impl<!neg>(a, has_changed);
  }

  template <class Mem>
  CUDA void preprocess(A&, BInc<Mem>&) {}

  CUDA NI void print(const A& a) const {
    left.print(a);
    if constexpr(neg) {
      printf(" != ");
    }
    else {
      printf(" == ");
    }
    right.print(a);
  }

  template <class Env>
  CUDA NI TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    auto lf = left.deinterpret(a, env, apc);
    auto rf = right.deinterpret(a, env, apc);
    return TFormula<typename Env::allocator_type>::make_binary(std::move(lf), neg ? NEQ : EQ, std::move(rf), apc, env.get_allocator());
  }

  CUDA size_t length() const { return 1 + left.length() + right.length(); }
};

template<class AD, class Allocator>
using Disequality = Equality<AD, Allocator, true>;

/**
 * A formula can occur in a term, e.g., `(x = 2) + (y = 2) + (z = 2) >= 2`
 * In that case, the entailment of the formula is mapped onto a sublattice of `U` supporting initialization from `0` and `1`.
 * A term can occur in a formula, as it is usually done.
 * Therefore, a formula is both a formula and a term.
 *
 * A logical formula is turned into a term by mapping their satisfiability to and from a sublattice of `U` representing Boolean values:
 *    - Consists of four distinct values \f$\{[\![x = 0]\!]_U \sqcap [\![x = 1]\!]_U, [\![x = 0]\!]_U, [\![x = 1]\!]_U, \top_U \}\f$.
 *    - With \f$\{[\![x = 0]\!]_U \sqcap [\![x = 1]\!]_U \f$ meaning neither true or false yet (e.g., unknown), \f$\{[\![x = 0]\!]_U \f$ modelling falsity, \f$ [\![x = 1]\!]_U \f$ modelling truth, and \f$ \top_U \f$ a logical statement both true and false (i.e., one of the variable is top).
*/
template <class AD, class Allocator>
class Formula {
public:
  using A = AD;
  using allocator_type = Allocator;
  using this_type = Formula<A, allocator_type>;
  using U = typename A::local_universe;
  using term_type = Term<A, allocator_type>;
  using this_ptr = battery::unique_ptr<this_type, allocator_type>;

  using PVarLit = VariableLiteral<A, false>;
  using NVarLit = VariableLiteral<A, true>;
  using PLOP = PLatticeOrderPredicate<A, allocator_type>;
  using NLOP = NLatticeOrderPredicate<A, allocator_type>;
  using Conj = Conjunction<A, allocator_type>;
  using Disj = Disjunction<A, allocator_type>;
  using Bicond = Biconditional<A, allocator_type>;
  using Imply = Implication<A, allocator_type>;
  using Xor = ExclusiveDisjunction<A, allocator_type>;
  using Eq = Equality<A, allocator_type>;
  using Neq = Disequality<A, allocator_type>;
  using AE = AbstractElement<A, allocator_type>;

  static constexpr size_t IPVarLit = 0;
  static constexpr size_t INVarLit = IPVarLit + 1;
  static constexpr size_t ITrue = INVarLit + 1;
  static constexpr size_t IFalse = ITrue + 1;
  static constexpr size_t IPLOP = IFalse + 1;
  static constexpr size_t INLOP = IPLOP + 1;
  static constexpr size_t IConj = INLOP + 1;
  static constexpr size_t IDisj = IConj + 1;
  static constexpr size_t IBicond = IDisj + 1;
  static constexpr size_t IImply = IBicond + 1;
  static constexpr size_t IXor = IImply + 1;
  static constexpr size_t IEq = IXor + 1;
  static constexpr size_t INeq = IEq + 1;
  static constexpr size_t IAE = INeq + 1;

  template <class A2, class Alloc2>
  friend class Formula;

private:
  using VFormula = battery::variant<
    PVarLit,
    NVarLit,
    True<A>,
    False<A>,
    PLOP,
    NLOP,
    Conj,
    Disj,
    Bicond,
    Imply,
    Xor,
    Eq,
    Neq,
    AE
  >;

  VFormula formula;

  template <size_t I, class FormulaType, class A2, class Alloc2>
  CUDA static VFormula create_one(const Formula<A2, Alloc2>& other, const allocator_type& allocator) {
    return VFormula::template create<I>(FormulaType(battery::get<I>(other.formula), allocator));
  }

  template <class A2, class Alloc2>
  CUDA NI static VFormula create(const Formula<A2, Alloc2>& other, const allocator_type& allocator) {
    switch(other.formula.index()) {
      case IPVarLit: return create_one<IPVarLit, PVarLit>(other, allocator);
      case INVarLit: return create_one<INVarLit, NVarLit>(other, allocator);
      case ITrue: return create_one<ITrue, True<A>>(other, allocator);
      case IFalse: return create_one<IFalse, False<A>>(other, allocator);
      case IPLOP: return create_one<IPLOP, PLOP>(other, allocator);
      case INLOP: return create_one<INLOP, NLOP>(other, allocator);
      case IConj: return create_one<IConj, Conj>(other, allocator);
      case IDisj: return create_one<IDisj, Disj>(other, allocator);
      case IBicond: return create_one<IBicond, Bicond>(other, allocator);
      case IImply: return create_one<IImply, Imply>(other, allocator);
      case IXor: return create_one<IXor, Xor>(other, allocator);
      case IEq: return create_one<IEq, Eq>(other, allocator);
      case INeq: return create_one<INeq, Neq>(other, allocator);
      case IAE: return create_one<IAE, AE>(other, allocator);
      default:
        printf("BUG: formula not handled.\n");
        assert(false);
        return VFormula::template create<IFalse>(False<A>());
    }
  }

  CUDA Formula(VFormula&& formula): formula(std::move(formula)) {}
public:
  template <class F>
  CUDA NI auto forward(F&& f) const {
    switch(formula.index()) {
      case IPVarLit: return f(battery::get<IPVarLit>(formula));
      case INVarLit: return f(battery::get<INVarLit>(formula));
      case ITrue: return f(battery::get<ITrue>(formula));
      case IFalse: return f(battery::get<IFalse>(formula));
      case IPLOP: return f(battery::get<IPLOP>(formula));
      case INLOP: return f(battery::get<INLOP>(formula));
      case IConj: return f(battery::get<IConj>(formula));
      case IDisj: return f(battery::get<IDisj>(formula));
      case IBicond: return f(battery::get<IBicond>(formula));
      case IImply: return f(battery::get<IImply>(formula));
      case IXor: return f(battery::get<IXor>(formula));
      case IEq: return f(battery::get<IEq>(formula));
      case INeq: return f(battery::get<INeq>(formula));
      case IAE: return f(battery::get<IAE>(formula));
      default:
        printf("BUG: formula not handled.\n");
        assert(false);
        return f(False<A>());
    }
  }

  template <class F>
  CUDA NI auto forward(F&& f) {
    switch(formula.index()) {
      case IPVarLit: return f(battery::get<IPVarLit>(formula));
      case INVarLit: return f(battery::get<INVarLit>(formula));
      case ITrue: return f(battery::get<ITrue>(formula));
      case IFalse: return f(battery::get<IFalse>(formula));
      case IPLOP: return f(battery::get<IPLOP>(formula));
      case INLOP: return f(battery::get<INLOP>(formula));
      case IConj: return f(battery::get<IConj>(formula));
      case IDisj: return f(battery::get<IDisj>(formula));
      case IBicond: return f(battery::get<IBicond>(formula));
      case IImply: return f(battery::get<IImply>(formula));
      case IXor: return f(battery::get<IXor>(formula));
      case IEq: return f(battery::get<IEq>(formula));
      case INeq: return f(battery::get<INeq>(formula));
      case IAE: return f(battery::get<IAE>(formula));
      default:
        printf("BUG: formula not handled.\n");
        assert(false);
        False<A> false_{};
        return f(false_);
    }
  }

  Formula() = default;
  Formula(this_type&&) = default;
  this_type& operator=(this_type&&) = default;

  template <class A2, class Alloc2>
  CUDA Formula(const Formula<A2, Alloc2>& other, const Allocator& allocator = Allocator())
    : formula(create(other, allocator))
  {}

  CUDA bool is(size_t kind) const {
    return formula.index() == kind;
  }

  CUDA size_t kind() const {
    return formula.index();
  }

  template <size_t I, class SubFormula>
  CUDA static this_type make(SubFormula&& sub_formula) {
    return Formula(VFormula::template create<I>(std::move(sub_formula)));
  }

  CUDA static this_type make_pvarlit(const AVar& avar) {
    return make<IPVarLit>(PVarLit(avar));
  }

  CUDA static this_type make_nvarlit(const AVar& avar) {
    return make<INVarLit>(NVarLit(avar));
  }

  CUDA static this_type make_true() {
    return make<ITrue>(True<A>{});
  }

  CUDA static this_type make_false() {
    return make<IFalse>(False<A>{});
  }

  CUDA static this_type make_plop(term_type&& left, U&& right) {
    return make<IPLOP>(PLOP(std::move(left), std::move(right)));
  }

  CUDA static this_type make_nlop(term_type&& left, U&& right, this_ptr&& not_f) {
    return make<INLOP>(NLOP(std::move(left), std::move(right), std::move(not_f)));
  }

  CUDA static this_type make_conj(this_ptr&& left, this_ptr&& right) {
    return make<IConj>(Conj(std::move(left), std::move(right)));
  }

  CUDA static this_type make_disj(this_ptr&& left, this_ptr&& right) {
    return make<IDisj>(Disj(std::move(left), std::move(right)));
  }

  CUDA static this_type make_bicond(this_ptr&& left, this_ptr&& right) {
    return make<IBicond>(Bicond(std::move(left), std::move(right)));
  }

  CUDA static this_type make_imply(this_ptr&& left, this_ptr&& right) {
    return make<IImply>(Imply(std::move(left), std::move(right)));
  }

  CUDA static this_type make_xor(this_ptr&& left, this_ptr&& right) {
    return make<IXor>(Xor(std::move(left), std::move(right)));
  }

  CUDA static this_type make_eq(term_type&& left, term_type&& right) {
    return make<IEq>(Eq(std::move(left), std::move(right)));
  }

  CUDA static this_type make_neq(term_type&& left, term_type&& right) {
    return make<INeq>(Neq(std::move(left), std::move(right)));
  }

  using tell_type = typename A::template tell_type<allocator_type>;
  using ask_type = typename A::template ask_type<allocator_type>;

  CUDA static this_type make_abstract_element(
    tell_type&& tellv, tell_type&& not_tellv,
    ask_type&& askv, ask_type&& not_askv)
  {
    return make<IAE>(AE(std::move(tellv), std::move(not_tellv), std::move(askv), std::move(not_askv)));
  }

  /** Call `refine` iff \f$ u \geq  [\![x = 1]\!]_U \f$ and `nrefine` iff \f$ u \geq  [\![x = 0]\!] \f$. */
  template <class Mem>
  CUDA void tell(A& a, const U& u, BInc<Mem>& has_changed) const {
    if(u >= U::eq_one()) { refine(a, has_changed); }
    else if(u >= U::eq_zero()) { nrefine(a, has_changed); }
  }

  /** Maps the truth value of \f$ \varphi \f$ to the Boolean sublattice of `U` (see above). */
  CUDA U project(const A& a) const {
    if(a.is_top()) { return U::top(); }
    if(ask(a)) { return U::eq_one(); }
    if(nask(a)) { return U::eq_zero(); }
    constexpr auto unknown = meet(U::eq_zero(), U::eq_one());
    return unknown;
  }

  CUDA void print(const A& a) const {
    forward([&](const auto& t) { t.print(a); });
  }

  template <class Env>
  CUDA TFormula<typename Env::allocator_type> deinterpret(const A& a, const Env& env, AType apc) const {
    return forward([&](const auto& t) { return t.deinterpret(a, env, apc); });
  }

  /** Given a formula \f$ \varphi \f$, the ask operation \f$ a \vDash \varphi \f$ holds whenever we can deduce \f$ \varphi \f$ from \f$ a \f$.
      More precisely, if \f$ \gamma(a) \subseteq [\![\varphi]\!]^\flat \f$, which implies that \f$ \varphi \f$ cannot remove further refine \f$ a \f$ since \f$ a \f$ is already more precise than \f$ \varphi \f$. */
  CUDA local::BInc ask(const A& a) const {
    return forward([&](const auto& t) { return t.ask(a); });
  }

  /** Similar to `ask` but for \f$ \lnot{\varphi} \f$. */
  CUDA local::BInc nask(const A& a) const {
    return forward([&](const auto& t) { return t.nask(a); });
  }

  /** An optional preprocessing operator that is only called at the root node.
   * This is because refinement operators are supposed to be stateless, hence are never backtracked.
   * It prepares the data of the formula to improve the refinement of subsequent calls to `refine`. */
  template <class Mem>
  CUDA void preprocess(A& a, BInc<Mem>& has_changed) {
    return forward([&](auto& t) { t.preprocess(a, has_changed); });
  }

  /** Refine the formula by supposing it must be true. */
  template <class Mem>
  CUDA void refine(A& a, BInc<Mem>& has_changed) const {
    return forward([&](const auto& t) { t.refine(a, has_changed); });
  }

  /** Refine the negation of the formula, hence we suppose the original formula needs to be false. */
  template <class Mem>
  CUDA void nrefine(A& a, BInc<Mem>& has_changed) const {
    return forward([&](const auto& t) { t.nrefine(a, has_changed); });
  }

  CUDA size_t length() const {
    return forward([](const auto& t) { return t.length(); });
  }
  // template <class Compiler>
  // CUDA void compile(const Compiler& compiler) {
  //   return forward([&](const auto& t){return t.compile(compiler);});
  // }
};

} // namespace pc
} // namespace lala

#endif
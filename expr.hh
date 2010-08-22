#ifndef bqt65asmExprHH
#define bqt65asmExprHH

#include <string>
#include <utility>
#include <list>
#include <set>
#include <cstdio>

class expression
{
public:
    virtual ~expression() { }
    virtual bool IsConst() const = 0;
    virtual long GetConst() const { return 0; }
    
    virtual const std::string Dump() const = 0;
    virtual void Optimize(expression*& self_ptr);
};
class expr_number: public expression
{
protected:
    long value;
public:
    expr_number(long v): value(v) { }
    
    virtual bool IsConst() const { return true; }
    virtual long GetConst() const { return value; }

    virtual const std::string Dump() const
    {
        char Buf[512];
        if(value < 0)
            std::sprintf(Buf, "$-%lX", -value);
        else
            std::sprintf(Buf, "$%lX", value);
        return Buf;
    }
};
class expr_label: public expression
{
protected:
    std::string name;
public:
    expr_label(const std::string& s): name(s) { }
    
    virtual bool IsConst() const { return false; }

    virtual const std::string Dump() const { return name; }
    const std::string& GetName() const { return name; }
};
class expr_unary: public expression
{
public:
    expression* sub;
public:
    expr_unary(expression *s): sub(s) { }
    virtual bool IsConst() const { return sub->IsConst(); }

    virtual ~expr_unary() { delete sub; }

    virtual void Optimize(expression*& self_ptr)
    {
         sub->Optimize(sub);
         expression::Optimize(self_ptr);
    }
private:
    expr_unary(const expr_unary &b);
    void operator= (const expr_unary &b);
};
class expr_binary: public expression
{
public:
    expression* left;
    expression* right;
public:
    expr_binary(expression *l, expression *r): left(l), right(r) { }
    virtual bool IsConst() const { return left->IsConst() && right->IsConst(); }

    virtual ~expr_binary() { delete left; delete right; }

    virtual void Optimize(expression*& self_ptr)
    {
         left->Optimize(left);
         right->Optimize(right);
         expression::Optimize(self_ptr);
    }
    
private:
    expr_binary(const expr_binary &b);
    void operator= (const expr_binary &b);
};

#define unary_class(classname, op, stringop) \
    class classname: public expr_unary \
    { \
    public: \
        classname(expression *s): expr_unary(s) { } \
        virtual long GetConst() const { return op sub->GetConst(); } \
    \
        virtual const std::string Dump() const \
        { return std::string(stringop) + "(" + sub->Dump() + ")"; } \
        \
        virtual void Optimize(expression*& self_ptr); \
    };

#define binary_class(classname, op, stringop) \
    class classname: public expr_binary \
    { \
    public: \
        classname(expression *l, expression *r): expr_binary(l, r) { } \
        virtual long GetConst() const { return left->GetConst() op right->GetConst(); } \
    \
        virtual const std::string Dump() const \
        { return std::string("(") + left->Dump() + stringop + right->Dump() + ")"; } \
    };

unary_class(expr_bitnot, ~, "not")
unary_class(expr_negate, -, "-")

binary_class(expr_mul,    *, "*")
binary_class(expr_div,    /, "/")
binary_class(expr_shl,   <<, " shl ")
binary_class(expr_shr,   >>, " shr ")
binary_class(expr_bitand, &, " and")
binary_class(expr_bitor,  |, " or ")
binary_class(expr_bitxor, ^, " xor ")

class sum_group: public expression
{
public:
    typedef std::pair<expression*, bool> elem_t;
    typedef std::list<elem_t> list_t;
    list_t contents;
public:
    sum_group(expression*l, expression*r, bool is_negative);
    virtual ~sum_group();

    virtual bool IsConst() const;
    virtual long GetConst() const;
    
    virtual const std::string Dump() const;
    virtual void Optimize(expression*& self_ptr);
private:
    friend class expr_negate;
    void Negate();
    
private:
    sum_group(const sum_group &b);
    void operator= (const sum_group &b);
};

void SubstituteExprLabel(expression*&, const std::string& name, long value, bool del_old=true);
void FindExprUsedLabels(const expression*, std::set<std::string>& labels);

#endif

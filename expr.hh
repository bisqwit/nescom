#ifndef bqt65asmExprHH
#define bqt65asmExprHH

#include <cstdio>
#include <string>
#include <utility>
#include <list>
#include <set>
#include <memory>

class expression
{
public:
    virtual ~expression() { }
    virtual bool IsConst() const = 0;
    virtual long GetConst() const { return 0; }

    virtual const std::string Dump() const = 0;
    virtual void Optimize(std::unique_ptr<expression>& self_ptr);
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
    std::unique_ptr<expression> sub;
public:
    expr_unary(std::unique_ptr<expression>&& s): sub(std::move(s)) { }
    virtual bool IsConst() const { return sub->IsConst(); }

    virtual void Optimize(std::unique_ptr<expression>& self_ptr)
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
    std::unique_ptr<expression> left;
    std::unique_ptr<expression> right;
public:
    expr_binary(std::unique_ptr<expression>&& l, std::unique_ptr<expression>&& r): left(std::move(l)), right(std::move(r)) { }
    virtual bool IsConst() const { return left->IsConst() && right->IsConst(); }

    virtual void Optimize(std::unique_ptr<expression>& self_ptr)
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
        classname(std::unique_ptr<expression>&& s): expr_unary(std::move(s)) { } \
        virtual long GetConst() const { return op sub->GetConst(); } \
    \
        virtual const std::string Dump() const \
        { return std::string(stringop) + "(" + sub->Dump() + ")"; } \
        \
        virtual void Optimize(std::unique_ptr<expression>& self_ptr); \
    };

#define binary_class(classname, op, stringop) \
    class classname: public expr_binary \
    { \
    public: \
        classname(std::unique_ptr<expression>&& l, std::unique_ptr<expression>&& r): expr_binary(std::move(l), std::move(r)) { } \
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
    typedef std::pair<std::unique_ptr<expression>, bool> elem_t;
    typedef std::list<elem_t> list_t;
    list_t contents;
public:
    sum_group(std::unique_ptr<expression>&& l, std::unique_ptr<expression>&& r, bool is_negative);

    virtual bool IsConst() const;
    virtual long GetConst() const;

    virtual const std::string Dump() const;
    virtual void Optimize(std::unique_ptr<expression>& self_ptr);
private:
    friend class expr_negate;
    void Negate();

private:
    sum_group(const sum_group &b);
    void operator= (const sum_group &b);
};

void SubstituteExprLabel(std::unique_ptr<expression>&, const std::string& name, long value);
void FindExprUsedLabels(const std::unique_ptr<expression>&, std::set<std::string>& labels);

std::pair<std::string, long> IsLabelSumExpression(const std::unique_ptr<expression>&);

#endif

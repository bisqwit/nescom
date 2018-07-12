#include "expr.hh"

void expression::Optimize(std::unique_ptr<expression>& self_ptr)
{
    if(self_ptr.get() != this) return;
    if(IsConst() && !dynamic_cast<class expr_number*> (this) )
    {
        self_ptr = std::unique_ptr<expression>(new expr_number(GetConst()));
    }
}

void expr_bitnot::Optimize(std::unique_ptr<expression>& self_ptr)
{
    expr_unary::Optimize(self_ptr);
    if(self_ptr.get() != this) return;
    if(expr_bitnot* tmp = dynamic_cast<expr_bitnot*> (sub.get()))
    {
        self_ptr = std::move(tmp->sub);
    }
}

void expr_negate::Optimize(std::unique_ptr<expression>& self_ptr)
{
    expr_unary::Optimize(self_ptr);
    if(self_ptr.get() != this) return;
    if(sum_group* s = dynamic_cast<sum_group*> (sub.get()))
    {
        s->Negate();
        self_ptr = std::move(sub);
        return;
    }
    if(expr_negate* tmp = dynamic_cast<expr_negate*> (sub.get()))
    {
        self_ptr = std::move(tmp->sub);
    }
}

void SubstituteExprLabel(std::unique_ptr<expression>& e, const std::string& name, long value)
{
    if(expr_label* l = dynamic_cast<expr_label*> (e.get()))
    {
        if(l->GetName() == name)
        {
            e = std::unique_ptr<expression>(new expr_number(value));
        }
    }
    else if(expr_unary* u = dynamic_cast<expr_unary*> (e.get()))
    {
        SubstituteExprLabel(u->sub, name, value);
    }
    else if(expr_binary* b = dynamic_cast<expr_binary*> (e.get()))
    {
        SubstituteExprLabel(b->left, name, value);
        SubstituteExprLabel(b->right, name, value);
    }
    else if(sum_group* s = dynamic_cast<sum_group*> (e.get()))
    {
        for(auto& child: s->contents)
        {
            SubstituteExprLabel(child.first, name, value);
        }
    }
}

void FindExprUsedLabels(const std::unique_ptr<expression>& e, std::set<std::string>& labels)
{
    if(const expr_label* l = dynamic_cast<const expr_label*> (e.get()))
    {
        labels.insert(l->GetName());
    }
    else if(const expr_unary* u = dynamic_cast<const expr_unary*> (e.get()))
    {
        FindExprUsedLabels(u->sub, labels);
    }
    else if(const expr_binary* b = dynamic_cast<const expr_binary*> (e.get()))
    {
        FindExprUsedLabels(b->left, labels);
        FindExprUsedLabels(b->right, labels);
    }
    else if(const sum_group* s = dynamic_cast<const sum_group*> (e.get()))
    {
        for(const auto& child: s->contents)
        {
            FindExprUsedLabels(child.first, labels);
        }
    }
}

sum_group::sum_group(std::unique_ptr<expression>&& l, std::unique_ptr<expression>&& r, bool is_negative)
{
     contents.emplace_back(std::move(l), false);
     contents.emplace_back(std::move(r), is_negative);
}

bool sum_group::IsConst() const
{
     for(const auto& child: contents)
          if(!child.first->IsConst())
              return false;
     return true;
}
long sum_group::GetConst() const
{
     long result = 0;
     for(const auto& child: contents)
          if(child.second)
               result -= child.first->GetConst();
          else
               result += child.first->GetConst();
     //std::fprintf(stderr, "Const value: $%lX of '%s'\n", result, Dump().c_str());
     return result;
}

const std::string sum_group::Dump() const
{
     std::string result = "(";
     for(const auto& child: contents)
     {
          result += child.second ? '-' : '+';
          result += child.first->Dump();
     }
     result += ')';
     return result;
}

void sum_group::Optimize(std::unique_ptr<expression>& self_ptr)
{
    long const_sum = 0;
    for(list_t::iterator i = contents.begin(); i != contents.end(); )
    {
        std::unique_ptr<expression>& e = i->first;
        e->Optimize(e);
        if(e->IsConst())
        {
            //std::fprintf(stderr, "Found const (%ld)%s\n", e->GetConst(), i->second?" (negated)":"");
            if(i->second)
                const_sum -= e->GetConst();
            else
                const_sum += e->GetConst();
            i = contents.erase(i);
            continue;
        }
        if(expr_negate* n = dynamic_cast<expr_negate*> (e.get()))
        {
            *i = elem_t(std::move(n->sub), !i->second);
            // Reanalyze same item
            continue;
        }
        if(sum_group* s = dynamic_cast<sum_group*> (e.get()))
        {
            if(i->second) s->Negate();
            contents.splice(contents.end(), std::move(s->contents));
            i = contents.erase(i);
            continue;
        }
        ++i;
    }
    if(contents.empty())
    {
        //std::fprintf(stderr, "Replaced with const sum=%ld\n", const_sum);
        self_ptr = std::unique_ptr<expression>(new expr_number(const_sum));
        return;
    }
    if(const_sum)
    {
        //std::fprintf(stderr, "Re-added const sum=%ld\n", const_sum);
        contents.emplace_back(new expr_number(const_sum), false);
    }
    if(contents.size() == 1)
    {
        // Replace self with the first element.
        auto& front = contents.front();
        auto ptr = std::move(front.first);
        if(front.second)
        {
            // Negate it if necessary.
            ptr = std::unique_ptr<expression>(new expr_negate(std::move(ptr)));
        }
        self_ptr = std::move(ptr);
    }
}

void sum_group::Negate()
{
    for(auto& child: contents)
        child.second = !child.second;
}

std::pair<std::string, long> IsLabelSumExpression(const std::unique_ptr<expression>& e)
{
    if(sum_group* s = dynamic_cast<sum_group*> (e.get()))
    {
        std::pair<std::string, long> result{ {}, {} };
        for(const auto& e: s->contents)
        {
            if(e.first->IsConst())
                { long val = e.first->GetConst(); if(e.second) val=-val; result.second += val; }
            else if(e.second || !result.first.empty())
                return {}; // Failed
            else
            {
                auto p = IsLabelSumExpression(e.first);
                if(p.first.empty()) return {}; // Failed
                result.first  = p.first;
                result.second += p.second;
            }
        }
        return result;
    }
    else if(expr_label* l = dynamic_cast<expr_label*> (e.get()))
    {
        return {l->GetName(), 0l};
    }
    else if(e.get()->IsConst())
        return {{}, e.get()->GetConst()};
    return {};
}

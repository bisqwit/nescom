#include "expr.hh"

void expression::Optimize(expression*& self_ptr)
{
    if(IsConst() && !dynamic_cast<class expr_number*> (this) )
    {
        self_ptr = new expr_number(GetConst());
        delete this;
    }
}

void expr_bitnot::Optimize(expression*& self_ptr)
{
    expr_unary::Optimize(self_ptr);
    if(self_ptr != this) return;
    if(expr_bitnot* tmp = dynamic_cast<expr_bitnot*> (sub))
    {
        self_ptr = tmp->sub;
        tmp->sub = NULL; /* to prevent it from being deleted */
        delete this; /* also deletes sub */
    }
}

void expr_negate::Optimize(expression*& self_ptr)
{
    expr_unary::Optimize(self_ptr);
    if(self_ptr != this) return;
    if(sum_group*s = dynamic_cast<sum_group*> (sub))
    {
        s->Negate();
        self_ptr = sub;
        
        sub = NULL;
        delete this;
        return;
    }
    if(expr_negate* tmp = dynamic_cast<expr_negate*> (sub))
    {
        self_ptr = tmp->sub;
        tmp->sub = NULL; /* to prevent it from being deleted */
        delete this; /* also deletes sub */
    }
}

void SubstituteExprLabel(expression*& e, const std::string& name, long value, bool del_old)
{
    bool del_children = del_old;

    if(expr_label* l = dynamic_cast<expr_label*> (e))
    {
        if(l->GetName() == name)
        {
            e = new expr_number(value);
            if(del_old) delete l;
        }
    }
    else if(expr_unary* u = dynamic_cast<expr_unary*> (e))
    {
        SubstituteExprLabel(u->sub, name, value, del_children);
    }
    else if(expr_binary* b = dynamic_cast<expr_binary*> (e))
    {
        SubstituteExprLabel(b->left, name, value, del_children);
        SubstituteExprLabel(b->right, name, value, del_children);
    }
    else if(sum_group* s = dynamic_cast<sum_group*> (e))
    {
        for(sum_group::list_t::iterator
            i = s->contents.begin(); i != s->contents.end(); ++i)
        {
            SubstituteExprLabel(i->first, name, value, del_children);
        }
    }
}

void FindExprUsedLabels(const expression* e, std::set<std::string>& labels)
{
    if(const expr_label* l = dynamic_cast<const expr_label*> (e))
    {
        labels.insert(l->GetName());
    }
    else if(const expr_unary* u = dynamic_cast<const expr_unary*> (e))
    {
        FindExprUsedLabels(u->sub, labels);
    }
    else if(const expr_binary* b = dynamic_cast<const expr_binary*> (e))
    {
        FindExprUsedLabels(b->left, labels);
        FindExprUsedLabels(b->right, labels);
    }
    else if(const sum_group* s = dynamic_cast<const sum_group*> (e))
    {
        for(sum_group::list_t::const_iterator
            i = s->contents.begin(); i != s->contents.end(); ++i)
        {
            FindExprUsedLabels(i->first, labels);
        }
    }
}

sum_group::sum_group(expression*l, expression*r, bool is_negative)
{
     contents.push_back(std::make_pair(l, false));
     contents.push_back(std::make_pair(r, is_negative));
}

bool sum_group::IsConst() const
{
     for(list_t::const_iterator i = contents.begin(); i != contents.end(); ++i)
          if(!i->first->IsConst()) return false;
     return true;
}
long sum_group::GetConst() const
{
     long result = 0;
     for(list_t::const_iterator i = contents.begin(); i != contents.end(); ++i)
          if(i->second)
               result -= i->first->GetConst();
          else
               result += i->first->GetConst();
     return result;
}

const std::string sum_group::Dump() const
{
     std::string result = "(";
     for(list_t::const_iterator i = contents.begin(); i != contents.end(); ++i)
     {
          result += i->second ? '-' : '+';
          result += i->first->Dump();
     }
     result += ')';
     return result;
}

void sum_group::Optimize(expression*& self_ptr)
{
    long const_sum = 0;
    for(list_t::iterator j,i = contents.begin(); i != contents.end(); i=j)
    {
        j=i; ++j;
    
        expression*& e = i->first;
        e->Optimize(e);
        if(expr_negate* n = dynamic_cast<expr_negate *> (e))
        {
            i->second = !i->second;
            
            e = n->sub;
            n->sub = NULL; delete n;
        }
        if(sum_group* s = dynamic_cast<sum_group *> (e))
        {
            if(i->second) s->Negate();
            
            contents.insert(contents.end(), s->contents.begin(), s->contents.end());
            
            contents.erase(i);
        }
        else if(e->IsConst())
        {
            if(i->second)
                const_sum -= e->GetConst();
            else
                const_sum += e->GetConst();
            contents.erase(i);
        }
    }
    if(contents.empty())
    {
        self_ptr = new expr_number(const_sum);
        delete this;
        return;
    }
    if(const_sum)
    {
        contents.push_back(std::make_pair(new expr_number(const_sum), false));
    }
    if(contents.size() == 1)
    {
        // Replace self with the first element.
        list_t::iterator i = contents.begin();
        self_ptr = i->first;
        if(i->second)
        {
            // Negate it if necessary.
            self_ptr = new expr_negate(self_ptr);
        }
        contents.erase(i);
        delete this;
    }
}

void sum_group::Negate()
{
    for(list_t::iterator i = contents.begin(); i != contents.end(); ++i)
        i->second = !i->second;
}

sum_group::~sum_group()
{
    for(list_t::const_iterator i = contents.begin(); i != contents.end(); ++i)
        delete i->first;
}

//Ayush Hitesh Soneria
//170192

#include<types.h>
#include<mmap.h>

/**
 * Function will invoked whenever there is page fault. (Lazy allocation)
 * 
 * For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
 */
int vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{
    int fault_fixed = -1;
    struct vm_area * t = current->vm_area;
    while(t!=NULL)
    {
        if(t->vm_start <= addr && t->vm_end >= addr)
            break;
        t = t->vm_next;
    }
    if(t==NULL)
        return -1;
    asm volatile ("invlpg (%0);" 
                    :: "r"(addr) 
                    : "memory");   // Flush TLB
    if(error_code == 4)
    {
        //read access if no -1
        if(t->access_flags & PROT_READ == 0)
            return -1;
        else
        {
            //for(u64 vaddr = t->vm_start; vaddr < t->vm_end; vaddr += PAGE_SIZE)
            //{
                map_physical_page(osmap(current->pgd), addr, t->access_flags, 0);
            //}
            fault_fixed = 1;
        }
    }
    else if(error_code == 6)
    {
        if(t->access_flags & PROT_WRITE == 0)
            return -1;
        else
        {
            //for(u64 vaddr = t->vm_start; vaddr < t->vm_end; vaddr += PAGE_SIZE)
            //{
                map_physical_page(osmap(current->pgd), addr, t->access_flags, 0);
            //}
            fault_fixed = 1;
        }
    }
    return fault_fixed;
}

/**
 * mprotect System call Implementation.
 */
int vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{
    length = (length%PAGE_SIZE)? (length/PAGE_SIZE +1)*PAGE_SIZE : length;
    if((addr < MMAP_AREA_START) || ((addr+length)>MMAP_AREA_END))
            return -1;
    int isValid = 0;
    if(current->vm_area == NULL)
        return -1;
    struct vm_area * t = current->vm_area;
    struct vm_area * s = NULL;
    struct vm_area * s2 = NULL;
    int er=0;
    while(t!= NULL)
    {
        if(t->vm_start < addr && t->vm_end > addr)
        {
            s = t;
            er=1;
            break;
        }
        t = t->vm_next;
    }
    if(er==0)
        return -1;
    t = current->vm_area;
    er=0;
    while(t != NULL)
    {
        if((t->vm_start < (addr + length)) && (t->vm_end > (addr+length)))
        {
            s2 = t;
            er=1;
            break;
        }   
        t = t->vm_next;
    }
    if(er==0)
        return -1;
    t=s;
    while(t!=s2)
    {
        if(t->vm_end != t->vm_next->vm_start) 
        {
            return -1;
        }
        t=t->vm_next;
    }
    t = current->vm_area;
    int count=0;
    while(t!=NULL)
    {
        count++;
        t = t->vm_next;
    }
    if(current->vm_area != NULL)
    {
        struct vm_area * x = NULL;
        struct vm_area * y = current->vm_area;
        //int er=0;
        while (y != NULL) 
        {
            if(addr <= y->vm_start && y->vm_end <= addr + length) 
            {
                //this vm_area completely lies in the range
                //hence change this whole vm area     
                y->access_flags = prot;
                //task2--
                for(u64 vaddr = y->vm_start; vaddr < y->vm_end; vaddr += PAGE_SIZE)
                {
                    u64* t = get_user_pte(current,vaddr,0);
                    if(prot & PROT_WRITE)
                    {
                        *t = *t | (1u << 1);
                    } 
                    else 
                    {
                        *t = *t & ~(1u << 1);
                    }
                }
                //task2--          
            } 
            else if (addr <= y->vm_start && y->vm_start < addr + length) 
            {
                //the front part of this vm_area lies in the range
                count++;
                if(count>128)
                    return -1;
                struct vm_area * temp = alloc_vm_area();
                temp->vm_start = y->vm_start;
                temp->vm_end = addr + length;
                temp->access_flags = prot;
                x->vm_next = temp;
                temp->vm_next = y;
                y->vm_start = addr + length;
                //task2--
                for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                {
                    u64* t = get_user_pte(current,vaddr,0);
                    if(prot & PROT_WRITE)
                    {
                        *t = *t | (1u << 1);
                    } 
                    else 
                    {
                        *t = *t & ~(1u << 1);
                    }
                }
                //task2-- 

            } 
            else if (y->vm_start < addr && addr + length < y->vm_end) 
            {
                //the middle part of this vm_area lies in the range
                count++;
                count++;
                if(count>128)
                    return -1;
                struct vm_area * temp = alloc_vm_area();
                struct vm_area * temp2 = alloc_vm_area();
                temp->vm_start = addr;
                temp->vm_end = addr+length;
                temp2->vm_start = addr + length;
                temp2->vm_end = y->vm_end;
                temp->vm_next = temp2;
                temp2->vm_next = y->vm_next;
                y->vm_end = addr;
                y->vm_next = temp;
                temp->access_flags = prot;
                temp2->access_flags = y->access_flags;
                //task2--
                for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                {
                    u64* t = get_user_pte(current,vaddr,0);
                    if(prot & PROT_WRITE)
                    {
                        *t = *t | (1u << 1);
                    } 
                    else 
                    {
                        *t = *t & ~(1u << 1);
                    }
                }
                //task2--
            } 
            else if (y->vm_start < addr && addr < y->vm_end) 
            {
                //the end par of this vm_area lies in the range
                count++;
                if(count>128)
                    return -1;
                struct vm_area * temp = alloc_vm_area();
                temp->vm_start = addr;
                temp->vm_end = y->vm_end;
                temp->access_flags = prot;
                temp->vm_next = y->vm_next;
                y->vm_next = temp;
                y->vm_end= addr;
                //task2--
                for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                {
                    u64* t = get_user_pte(current,vaddr,0);
                    if(prot & PROT_WRITE)
                    {
                        *t = *t | (1u << 1);
                    } 
                    else 
                    {
                        *t = *t & ~(1u << 1);
                    }
                }
                //task2--
            }
            x = y;
            y = y->vm_next;
        }

        y = current->vm_area;
        x = NULL;
        while (y != NULL) 
        {
            if (x != NULL) 
            {
                //check whether to merge this vm_area with the previous one
                if (x->vm_end == y->vm_start && x->access_flags == y->access_flags) 
                {
                    //merge this vm_Area with the previous one
                    x->vm_end = y->vm_end;
                    x->vm_next = y->vm_next;
                    dealloc_vm_area(y);
                    count--;
                    //remove the pages allocated for this area
                }
            }
            x = y;
            y = y->vm_next;
        }
    }
    return isValid;
}
/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{
    length = (length%PAGE_SIZE)? (length/PAGE_SIZE +1)*PAGE_SIZE : length;
    long ret_addr = -1;
    struct vm_area * t = current->vm_area;
    int count=0;
    while(t!=NULL)
    {
        count++;
        t = t->vm_next;
    }
    if (!addr) 
    {
        struct vm_area* y = current->vm_area;
        struct vm_area* x = NULL;
        u64 curr_addr = MMAP_AREA_START;
        if(y==NULL)
        {
            count++;
            if(count>128)
                return -1;
            struct vm_area * temp = alloc_vm_area();
            temp->vm_start = curr_addr;
            temp->vm_end = curr_addr + length;
            temp->access_flags = prot;
            temp->vm_next = y;
            current->vm_area =temp;
            ret_addr = temp->vm_start;
            //task2--
            if(flags == MAP_POPULATE)
            {
                for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                {
                    map_physical_page(osmap(current->pgd), vaddr, prot, 0);
                    //get_user_pte(current,vaddr,0);
                }
            }
            //task2--
            return ret_addr;
        }
        while(y!=NULL) 
        {
            if(curr_addr + length <= y->vm_start) 
            {
                //create a new vm_area
                count++;
                if(count>128)
                    return -1;
                struct vm_area * temp = alloc_vm_area();
                temp->vm_start = curr_addr;
                temp->vm_end = curr_addr + length;
                temp->access_flags = prot;
                temp->vm_next = y;
                if(y==current->vm_area)
                    current->vm_area = temp;
                else if(x!=NULL)
                    x->vm_next = temp;
                ret_addr = temp->vm_start;
                //task2--
                if(flags == MAP_POPULATE)
                {
                    for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                    {
                        map_physical_page(osmap(current->pgd), vaddr, prot, 0);
                        //get_user_pte(current,vaddr,0);
                    }
                }
                //task2--
                break;
            }
            curr_addr = y->vm_end;
            x = y;
            y = y->vm_next;     
        }
        if(y==NULL)
        {
            if(x->vm_end + length <= MMAP_AREA_END) 
            {
                //create a new vm_area
                count++;
                if(count>128)
                    return -1;
                struct vm_area * temp = alloc_vm_area();
                temp->vm_start = x->vm_end;
                temp->vm_end = x->vm_end + length;
                temp->access_flags = prot;
                temp->vm_next = NULL;
                if(y==current->vm_area)
                    current->vm_area = temp;
                else if(x!=NULL)
                    x->vm_next = temp;
                ret_addr = temp->vm_start;
                //task2--
                if(flags == MAP_POPULATE)
                {
                    for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                    {
                        map_physical_page(osmap(current->pgd), vaddr, prot, 0);
                        //get_user_pte(current,vaddr,0);
                    }
                }
                //task2--
            }
            // else
            //     return -1;
        }
        y = current->vm_area;
        x = NULL;
        while (y != NULL) 
        {
            if (x != NULL) 
            {
                //check whether to merge this vm_area with the previous one
                if (x->vm_end == y->vm_start && x->access_flags == y->access_flags) 
                {
                    //merge this vm_Area with the previous one
                    x->vm_end = y->vm_end;
                    x->vm_next = y->vm_next;
                    dealloc_vm_area(y);
                    count--;
                    y = x->vm_next;
                    continue;
                    //remove the pages allocated for this area
                }
            }
            x = y;
            y = y->vm_next;
        }
    }
    else
    {
        //with hint address
        struct vm_area* y = current->vm_area;
        struct vm_area* x = NULL;
        u64 curr_addr = MMAP_AREA_START;
        int er=0;
        if(y==NULL)
        {
            count++;
            if(count>128)
                return -1;
            struct vm_area * temp = alloc_vm_area();
            temp->vm_start = addr;
            temp->vm_end = addr + length;
            temp->access_flags = prot;
            temp->vm_next = y;
            current->vm_area =temp;
            ret_addr = temp->vm_start;
            er=1;
            //task2--
            if(flags == MAP_POPULATE)
            {
                for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                {
                    map_physical_page(osmap(current->pgd), vaddr, prot, 0);
                    //get_user_pte(current,vaddr,0);
                }
            }
            //task2--
            return ret_addr;
        }
        while(y!=NULL) 
        {
            if(curr_addr <= addr && addr + length <= y->vm_start) 
            {
                //create a new vm_area
                count++;
                if(count>128)
                    return -1;
                struct vm_area * temp = alloc_vm_area();
                temp->vm_start = addr;
                temp->vm_end = addr + length;
                temp->access_flags = prot;
                temp->vm_next = y;
                if(y==current->vm_area)
                    current->vm_area = temp;
                else if(x!=NULL)
                    x->vm_next = temp;
                ret_addr = temp->vm_start;
                er=1;
                //task2--
                if(flags == MAP_POPULATE)
                {
                    for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                    {
                        map_physical_page(osmap(current->pgd), vaddr, prot, 0);
                        //get_user_pte(current,vaddr,0);
                    }
                }
                //task2--
                break;
            }
            curr_addr = y->vm_end;
            x = y;
            y = y->vm_next;
        }
        if(y==NULL)
        {
            if (x->vm_end <= addr && addr + length <= MMAP_AREA_END) 
            {
                //create a new vm_area
                count++;
                if(count>128)
                    return -1;
                struct vm_area * temp = alloc_vm_area();
                temp->vm_start = addr;
                temp->vm_end = addr + length;
                temp->access_flags = prot;
                temp->vm_next = NULL;
                if(y==current->vm_area)
                    current->vm_area = temp;
                else if(x!=NULL)
                    x->vm_next = temp;
                er=1;
                ret_addr = temp->vm_start;
                //task2--
                if(flags == MAP_POPULATE)
                {
                    for(u64 vaddr = temp->vm_start; vaddr < temp->vm_end; vaddr += PAGE_SIZE)
                    {
                        map_physical_page(osmap(current->pgd), vaddr, prot, 0);
                        //get_user_pte(current,vaddr,0);
                    }
                }
                //task2--
            }
        }
        if(er==0)
        {
            if(flags == MAP_FIXED)
                return -1;
            else
                return vm_area_map(current,0,length,prot,flags);
        }
        y = current->vm_area;
        x = NULL;
        while (y != NULL) 
        {
            if (x != NULL) 
            {
                //check whether to merge this vm_area with the previous one
                if (x->vm_end == y->vm_start && x->access_flags == y->access_flags) 
                {
                    //merge this vm_Area with the previous one
                    x->vm_end = y->vm_end;
                    x->vm_next = y->vm_next;
                    dealloc_vm_area(y);
                    count--;
                    y = x->vm_next;
                    continue;
                    //remove the pages allocated for this area
                }
            }
            x = y;
            y = y->vm_next;
        }
    }
    return ret_addr;
}
/**
 * munmap system call implemenations
 */

int vm_area_unmap(struct exec_context *current, u64 addr, int length)
{
    length = (length%PAGE_SIZE)? (length/PAGE_SIZE +1)*PAGE_SIZE : length;
    int isValid = 0;
    struct vm_area * t = current->vm_area;
    int count=0;
    while(t!=NULL)
    {
        count++;
        t = t->vm_next;
    }
    struct vm_area * x = NULL;
    struct vm_area * y = current->vm_area;
    while (y != NULL) 
    {
        if(addr <= y->vm_start && y->vm_end <= addr + length) 
        {
            //this vm_area completely lies in the unmap range
            //hence delete this whole vm area
            if(y==current->vm_area)
            {
                current->vm_area = NULL;
            }
            else if(x!=NULL)
            {
                x->vm_next = y->vm_next;
            }
            //task2--
            for(u64 vaddr = y->vm_start; vaddr < y->vm_end; vaddr += PAGE_SIZE)
            {
                do_unmap_user(current,vaddr);
                //get_user_pte(current,vaddr,0);
            }
            //task2--
            dealloc_vm_area(y);
            count--;
            //return 0;
        } 
        else if (addr <= y->vm_start && y->vm_start < addr + length) 
        {
            //the front part of this vm_area lies in the unmap range
            y->vm_start = addr + length;
            //return 0;
        } 
        else if (y->vm_start < addr && addr + length < y->vm_end) 
        {
            //the middle part of this vm_area lies in the unmap range
            count++;//omnly once bcuz of dealloc later
            if(count>128)
                return -1;
            struct vm_area * temp = alloc_vm_area();
            struct vm_area * temp2 = alloc_vm_area();
            temp->vm_start = y->vm_start;
            temp->vm_end = addr;
            temp2->vm_start = addr + length;
            temp2->vm_end = y->vm_end;
            temp->vm_next = temp2;
            temp2->vm_next = y->vm_next;
            temp->access_flags = y->access_flags;
            temp2->access_flags = y->access_flags;
            if(y==current->vm_area)
            {
                current->vm_area = temp;
            }
            else if(x!=NULL)
            {
                x->vm_next = temp;
            }
            //task2--
            for(u64 vaddr = addr; vaddr < addr + length; vaddr += PAGE_SIZE)
            {
                do_unmap_user(current,vaddr);
                //get_user_pte(current,vaddr,0);
            }
            //task2--
            dealloc_vm_area(y);
            //return 0;
        } 
        else if (y->vm_start < addr && addr < y->vm_end) 
        {
            //the end par of this vm_area lies in the unmap range
            y->vm_end = addr;
            //return 0;
        }
        x = y;
        y = y->vm_next;
    }

    y = current->vm_area;
    x = NULL;
    while (y != NULL) 
    {
        if (x != NULL) 
        {
            //check whether to merge this vm_area with the previous one
            if (x->vm_end == y->vm_start && x->access_flags == y->access_flags) 
            {
                //merge this vm_Area with the previous one
                x->vm_end = y->vm_end;
                x->vm_next = y->vm_next;
                dealloc_vm_area(y);
                count--;
                //return 0;
                //remove the pages allocated for this area
            }
        }
        x = y;
        y = y->vm_next;
    }
    //}
    return isValid;
}

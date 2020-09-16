//Ayush Hitesh Soneria
//170192

#include <cfork.h>
#include <page.h>
#include <mmap.h>



/* You need to implement cfork_copy_mm which will be called from do_cfork in entry.c. Don't remove copy_os_pts()*/
void cfork_copy_mm(struct exec_context *child, struct exec_context *parent ){

    // copy_os_pts(parent->pgd, child->pgd);

    // return;
    void *os_addr;
   u64 vaddr; 
   struct mm_segment *seg;

   child->pgd = os_pfn_alloc(OS_PT_REG);

   os_addr = osmap(child->pgd);
   bzero((char *)os_addr, PAGE_SIZE);
   
   //CODE segment
   seg = &parent->mms[MM_SEG_CODE];
   for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      if(parent_pte)
           install_ptable((u64) os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
   } 
   //RODATA segment
   
   seg = &parent->mms[MM_SEG_RODATA];
   for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      if(parent_pte)
           install_ptable((u64)os_addr, seg, vaddr, (*parent_pte & FLAG_MASK) >> PAGE_SHIFT);   
   } 
   
   //DATA segment
  seg = &parent->mms[MM_SEG_DATA];
  for(vaddr = seg->start; vaddr < seg->next_free; vaddr += PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
      if(parent_pte){
           u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0);  //Returns the blank page  
           pfn = (u64)osmap(pfn);
           memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
      }
  } 
  
  //STACK segment
  seg = &parent->mms[MM_SEG_STACK];
  for(vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE){
      u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      
     if(parent_pte){
           u64 pfn = install_ptable((u64)os_addr, seg, vaddr, 0);  //Returns the blank page  
           pfn = (u64)osmap(pfn);
           memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
      }
  } 
 
   
  copy_os_pts(parent->pgd, child->pgd); 
  return; 
    
}

/* You need to implement cfork_copy_mm which will be called from do_vfork in entry.c.*/
void vfork_copy_mm(struct exec_context *child, struct exec_context *parent ){
  	//STACK segment
  	parent->state = WAITING;
	struct mm_segment *seg = &parent->mms[MM_SEG_STACK];
	u64 x;
	for(u64 vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE)
	{
    	u64 *parent_pte =  get_user_pte(parent, vaddr, 0);
      	x = seg->end - seg->next_free;
	    if(parent_pte)
	    {
           u64 pfn = install_ptable((u64)osmap(child->pgd), seg, vaddr-x, 0);  //Returns the blank page  
           pfn = (u64)osmap(pfn);
           memcpy((char *)pfn, (char *)(*parent_pte & FLAG_MASK), PAGE_SIZE); 
	    }
	    asm volatile ("invlpg (%0);" 
                    :: "r"(vaddr) 
                    : "memory");   // Flush TLB
	}	
	struct mm_segment *seg2 = &child->mms[MM_SEG_STACK];
	child->regs.entry_rsp = child->regs.entry_rsp - x;
	child->regs.rbp = child->regs.rbp - x;
	seg2->end = seg->end - x;
	seg2->next_free = seg->next_free - x;
}

/*You need to implement handle_cow_fault which will be called from do_page_fault 
incase of a copy-on-write fault

* For valid acess. Map the physical page 
 * Return 1
 * 
 * For invalid access,
 * Return -1. 
*/

int handle_cow_fault(struct exec_context *current, u64 cr2){

    return -1;
}

/* You need to handle any specific exit case for vfork here, called from do_exit*/
void vfork_exit_handle(struct exec_context *ctx)
{
	struct exec_context* parent = get_ctx_by_pid(ctx->ppid);
	if(parent->pgd != ctx->pgd) return;
	parent->state = READY;
	if(ctx->vm_area == NULL) parent->vm_area = NULL;
	struct mm_segment *seg = &parent->mms[MM_SEG_STACK];
	u64 x = seg->end - seg->next_free;
	for(u64 vaddr = seg->end - PAGE_SIZE; vaddr >= seg->next_free; vaddr -= PAGE_SIZE)
	{
        do_unmap_user(ctx,vaddr-x);
    }
    return;
}
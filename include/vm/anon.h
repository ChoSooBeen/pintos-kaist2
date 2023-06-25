#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
     //swap out되어 할당된 슬롯 번호
     //swap out된 페이지는 요구 페이징에 의해 다시 메모리 로드 
    uint32_t slot_num;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif

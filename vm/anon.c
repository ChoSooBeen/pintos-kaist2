/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	list_init(&swap_table);
	lock_init(&swap_table_lock);

	disk_sector_t swap_size = disk_size(swap_disk) / 8;
	for (disk_sector_t i = 0; i < swap_size; i++) {
		struct slot *slot = (struct slot *)malloc(sizeof(struct slot));
		slot->page = NULL;
		slot->slot_num = i;
		lock_acquire(&swap_table_lock);
		list_push_back(&swap_table, &slot->swap_elem);
		lock_release(&swap_table_lock);
	}
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	anon_page->slot_num = -1;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	disk_sector_t page_slot = anon_page->slot_num;
	struct slot *slot;

	lock_acquire(&swap_table_lock);
	for (struct list_elem *e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)) {
		slot = list_entry(e, struct slot, swap_elem);
		if(slot->slot_num == page_slot) {
			for (int i = 0; i < 8; i++) {
				disk_read(swap_disk, page_slot * 8 + i, kva + DISK_SECTOR_SIZE * i);
			}
			slot->page = NULL;
			anon_page->slot_num = -1;
			lock_release(&swap_table_lock);
			return true;
		}
	}
	lock_release(&swap_table_lock);
	return false;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	if(page == NULL) {
		return false;
	}
	struct anon_page *anon_page = &page->anon;
	struct slot *slot;

	lock_acquire(&swap_table_lock);
	for (struct list_elem *e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)) {
		slot = list_entry(e, struct slot, swap_elem);
		if(slot->page == NULL) {
			for (int i = 0; i < 8; i++) {
				disk_write(swap_disk, slot->slot_num * 8 + i, page->va + DISK_SECTOR_SIZE * i);
			}
			slot->page = page;
			anon_page->slot_num = slot->slot_num;

			page->frame->page = NULL;
			page->frame = NULL;
			pml4_clear_page(thread_current()->pml4, page->va);
			lock_release(&swap_table_lock);
			return true;
		}
	}
	lock_release(&swap_table_lock);
	PANIC("insufficient swap space");
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	struct slot *slot;

	lock_acquire(&swap_table_lock);
	for (struct list_elem *e = list_begin(&swap_table); e != list_end(&swap_table); e = list_next(e)) {
		slot = list_entry(e, struct slot, swap_elem);
		if (slot->slot_num == anon_page->slot_num) {
			slot->page = NULL;
			break;
		}
	}
	lock_release(&swap_table_lock);
}

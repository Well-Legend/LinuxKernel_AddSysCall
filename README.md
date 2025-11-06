Linux Operating System Project 1
===
Download Kernel Source
---
```bash
# 進入 root 模式

sudo su

# 下載kernel

wget -P ~/ https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.137.tar.xz
```
輸入指令時請注意所處位置，他會下載到當前所處資料夾

### 解壓縮
```bash
# 把檔案解壓縮到 /usr/src 目錄底下

tar -xvf linux-5.15.137.tar.xz -C /usr/src
```

### 安裝相關套件
```bash
sudo apt update && sudo apt upgrade -y

# 這個套件可以幫我們 build 出 kernel-pakage

sudo apt install build-essential libncurses-dev libssl-dev libelf-dev bison flex -y
```

### 安裝vim
```bash
sudo apt install vim -y
```

### 清除安裝的package
```bash
sudo apt clean && sudo apt autoremove -y
```
---

Question 1
---
In this part, we use malloc() to allocate memory. At first, the virtual addresses (VAs) are reserved, but they do not yet have corresponding physical addresses (PAs).
Only when the program writes to a page does the operating system actually allocate a physical page.
This mechanism, where physical memory is allocated only on first access, is called lazy allocation.

### 專案說明
在現代作業系統中，每個執行中的程式（process）都擁有獨立的虛擬位址空間。
CPU與作業系統透過**頁表** (**page table**) 將**虛擬位址**（**Virtual Address, VA**）轉換為**實體位址**（**Physical Address, PA**）。

本實驗的目標是實作一個**新的 Linux system call**，用於查詢給定虛擬位址目前對應的實體位址，並觀察記憶體的 lazy allocation 行為。

### 實作
- **新增syscall**  
	```bash
	# 把目錄轉到剛剛解壓縮完的 kernel 檔案夾
	
	cd /usr/src/linux-5.15.137
	
	# 在裡面創建一個名叫 project1 的資料夾
	
	mkdir project1
	
	# 把目錄轉到 project1 資料夾
	
	cd project1
	```
 	***
 
- **建立新檔案**
	```bash
	vim my_get_physical_addresses.c
	```
	並填入 code
	```c
	#include <linux/kernel.h>
	#include <linux/syscalls.h>
	#include <linux/mm.h>
	#include <linux/mmap_lock.h>
	#include <linux/sched.h>
	#include <asm/pgtable.h>
	
	SYSCALL_DEFINE1(my_get_physical_addresses, unsigned long, vaddr)
	{
	    struct mm_struct *mm = current->mm;
	    unsigned long addr = vaddr, pa = 0;
	    pgd_t *pgd; p4d_t *p4d; pud_t *pud; pmd_t *pmd; pte_t *pte;
	
	    if (!mm) return 0;
	    if (addr >= TASK_SIZE_MAX) return 0;
	
	    mmap_read_lock(mm);
	
	    pgd = pgd_offset(mm, addr);
	    if (pgd_none(*pgd) || pgd_bad(*pgd)) goto out;
	    p4d = p4d_offset(pgd, addr);
	    if (p4d_none(*p4d) || p4d_bad(*p4d)) goto out;
	    pud = pud_offset(p4d, addr);
	    if (pud_none(*pud) || pud_bad(*pud)) goto out;
	    pmd = pmd_offset(pud, addr);
	    if (pmd_none(*pmd) || pmd_bad(*pmd)) goto out;
	
	    pte = pte_offset_map(pmd, addr);
	    if (!pte) goto out;
	    if (!pte_present(*pte)) { pte_unmap(pte); goto out; }
	
	    pa = ((unsigned long)pte_pfn(*pte) << PAGE_SHIFT) |
	         (addr & (PAGE_SIZE - 1));
	
	    pte_unmap(pte);
	
	out:
	    mmap_read_unlock(mm);
	    return pa;
	}
	```
	***

- **在 project1 資料夾中建立 Makefile**
	```bash
	vim Makefile
	```
 
	並填入 code

	```bash
	# 將my_get_physical_addresses.o編入kernel
	
	obj-y := my_get_physical_addresses.o 
	```
 	Makefile功能 : 告訴 make 如何編譯這些檔案  
	Makefile 的主要目的是讓 make 工具知道如何根據源代碼的變化來更新和編譯目標檔案，以及確保這些操作按照正確的順序執行。

	***
- **編輯原系統中的Makefile**  
  ```bash
  cd ..

  nano Makefile
  ```
  
  並找到這一行
  
  ```bash
  core-y			+= kernel/ certs/ mm/ fs/ ipc/ security/ crypto/
  ```
  
  並在其後加上 `project1/` 即
  
  ```bash
  core-y			+= kernel/ certs/ mm/ fs/ ipc/ security/ crypto/ project1/
  ```

	***
- **將系統呼叫對應的函數加入到系統呼叫的標頭檔中**  
  ```bash
  nano include/linux/syscalls.h
  ```

  並在`#endif`前加上
  ```bash
  asmlinkage long sys_my_get_physical_addresses(unsigned long __user *usr_ptr);
  ```

	當 assembly code 呼叫 C function，並且是以 stack 方式傳參數（parameter）時，在 C function 的 prototype 前面就要加上 "asmlinkage"。

	***
- **將syscall加入到Kernel的syscall table**  
  ```bash
  nano arch/x86/entry/syscalls/syscall_64.tbl
  ```
  拉至其底部。會發現一系列 x32 系統呼叫。滾動到其上方的section。並插入
  ```bash
  449	common	my_get_physical_addresses		sys_my_get_physical_addresses
  ```
  使用 Tab 來表示空格，勿使用空白鍵!!

	***
- **編譯設定**
  ```bash
  make clean
  cd
  cd /usr/src/linux-5.15.137/
  cp -v /boot/config-$(uname -r) .config
  make localmodconfig
  ```
  ```bash
  scripts/config --disable SYSTEM_TRUSTED_KEYS
  scripts/config --disable SYSTEM_REVOCATION_KEYS
  scripts/config --set-str CONFIG_SYSTEM_TRUSTED_KEYS ""
  scripts/config --set-str CONFIG_SYSTEM_REVOCATION_KEYS ""
  ```
  
  ***
- **編譯Kernel並重開機**
  ```bash
  make -j$(nproc)
  sudo make modules_install
  sudo make install
  sudo update-grub
  sudo reboot

  ***
- **檢查版本**
  ```bash
  uname -rs
  ```
  
### 測試驗證
```bash
gcc -O2 -o test_q1 test_q1.c
./test_q1.c
```

***
Question 2
---
In this part, you need to convert a C system call into a shared library so that it can be called from Python using ctypes library.

### 專案說明
在 Question 1 中，我們已經在 Linux Kernel 5.15.137 上實作完成一個自訂的 system call：
```bash
unsigned long my_get_physical_addresses(void *va)
```

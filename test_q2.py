import ctypes

# -----------------------------
# 載入自訂 .so
# -----------------------------
lib = ctypes.CDLL('/usr/src/linux-5.15.137/project1/lib_my_get_phy.so')
lib.my_get_physical_addresses.argtypes = [ctypes.c_void_p]
lib.my_get_physical_addresses.restype = ctypes.c_ulong

def virt2phys(addr):
    """VA -> PA"""
    return lib.my_get_physical_addresses(ctypes.c_void_p(addr))

# -----------------------------
# libc.sbrk 取得 heap break
# -----------------------------
libc = ctypes.CDLL("libc.so.6")
libc.sbrk.restype = ctypes.c_void_p
libc.sbrk.argtypes = [ctypes.c_long]

def get_heap_break():
    return libc.sbrk(0)

PAGE_SIZE = 4096

print("=== 初始 program break ===")
break_before = get_heap_break()
print(f"program break: {hex(break_before)}\n")

malloc_size = 4096  # 每次分配 4KB
buffers = []

# 逐次分配記憶體並觀察 break 變化
for i in range(50):
    buf = ctypes.create_string_buffer(malloc_size)
    buffers.append(buf)
    break_now = get_heap_break()

    if break_now != break_before:
        print(f"malloc {i+1}: break = {hex(break_now)}")
        print(f"  -> program break 增加了 {break_now - break_before} bytes")

        print("\n=== 新增 heap VA->PA ===")
        start_va = break_before
        end_va = break_now
        for va in range(start_va, end_va, PAGE_SIZE):
            pa = virt2phys(va)
            if pa == 0:
                pa_str = "未分配"
            else:
                pa_str = f"0x{pa:x}"
            print(f"VA: 0x{va:x} -> PA: {pa_str}")
        print()
        break_before = break_now

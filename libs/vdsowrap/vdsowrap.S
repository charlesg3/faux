.text
.code32
.globl  vdso_invoke
vdso_invoke:
  push  %ebp
  push  %ebx
  push  %edi
  push  %esi

  mov   0x14(%esp), %eax
  mov   0x18(%esp), %ebx
  mov   0x1c(%esp), %ecx
  mov   0x20(%esp), %edx
  mov   0x24(%esp), %esi
  mov   0x28(%esp), %edi
  mov   0x2c(%esp), %ebp
  call  *vdso_kernel_handler

  pop   %esi
  pop   %edi
  pop   %ebx
  pop   %ebp
  ret

.globl  vdso_entry
vdso_entry:
  push  %ebp
  push  %edi
  push  %esi
  push  %edx
  push  %ecx
  push  %ebx
  push  %eax
  call  vdso_entry_c
  pop   %ebx    // clobber
  pop   %ebx
  pop   %ecx
  pop   %edx
  pop   %esi
  pop   %edi
  pop   %ebp
  ret

.data
.globl  vdso_kernel_handler
vdso_kernel_handler:
  .long 0


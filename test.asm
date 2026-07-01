
build/d3d9_test.exe:	file format coff-x86-64

Disassembly of section .text:

0000000140001000 <print_out>:
140001000: 55                          	pushq	%rbp
140001001: 48 89 e5                    	movq	%rsp, %rbp
140001004: 48 83 ec 40                 	subq	$0x40, %rsp
140001008: 48 89 4d 10                 	movq	%rcx, 0x10(%rbp)
14000100c: c7 45 f4 00 00 00 00        	movl	$0x0, -0xc(%rbp)
140001013: b9 f5 ff ff ff              	movl	$0xfffffff5, %ecx       # imm = 0xFFFFFFF5
140001018: 48 8b 05 a9 40 00 00        	movq	0x40a9(%rip), %rax      # 0x1400050c8 <__imp_GetStdHandle>
14000101f: ff d0                       	callq	*%rax
140001021: 48 89 45 f8                 	movq	%rax, -0x8(%rbp)
140001025: 48 8b 45 10                 	movq	0x10(%rbp), %rax
140001029: 48 89 c1                    	movq	%rax, %rcx
14000102c: 48 8b 05 ad 40 00 00        	movq	0x40ad(%rip), %rax      # 0x1400050e0 <__imp_lstrlenA>
140001033: ff d0                       	callq	*%rax
140001035: 41 89 c0                    	movl	%eax, %r8d
140001038: 48 8d 4d f4                 	leaq	-0xc(%rbp), %rcx
14000103c: 48 8b 55 10                 	movq	0x10(%rbp), %rdx
140001040: 48 8b 45 f8                 	movq	-0x8(%rbp), %rax
140001044: 48 c7 44 24 20 00 00 00 00  	movq	$0x0, 0x20(%rsp)
14000104d: 49 89 c9                    	movq	%rcx, %r9
140001050: 48 89 c1                    	movq	%rax, %rcx
140001053: 48 8b 05 7e 40 00 00        	movq	0x407e(%rip), %rax      # 0x1400050d8 <__imp_WriteFile>
14000105a: ff d0                       	callq	*%rax
14000105c: 90                          	nop
14000105d: 48 83 c4 40                 	addq	$0x40, %rsp
140001061: 5d                          	popq	%rbp
140001062: c3                          	retq

0000000140001063 <_start>:
140001063: 55                          	pushq	%rbp
140001064: 48 89 e5                    	movq	%rsp, %rbp
140001067: 48 81 ec e0 00 00 00        	subq	$0xe0, %rsp
14000106e: 48 8d 05 8b 0f 00 00        	leaq	0xf8b(%rip), %rax       # 0x140002000 <__data_start__>
140001075: 48 89 c1                    	movq	%rax, %rcx
140001078: e8 83 ff ff ff              	callq	0x140001000 <print_out>
14000107d: b9 00 00 00 00              	movl	$0x0, %ecx
140001082: 48 8b 05 37 40 00 00        	movq	0x4037(%rip), %rax      # 0x1400050c0 <__imp_GetModuleHandleA>
140001089: ff d0                       	callq	*%rax
14000108b: 48 8d 0d 89 0f 00 00        	leaq	0xf89(%rip), %rcx       # 0x14000201b <__data_start__+0x1b>
140001092: 48 8d 15 95 0f 00 00        	leaq	0xf95(%rip), %rdx       # 0x14000202e <__data_start__+0x2e>
140001099: 48 c7 44 24 58 00 00 00 00  	movq	$0x0, 0x58(%rsp)
1400010a2: 48 89 44 24 50              	movq	%rax, 0x50(%rsp)
1400010a7: 48 c7 44 24 48 00 00 00 00  	movq	$0x0, 0x48(%rsp)
1400010b0: 48 c7 44 24 40 00 00 00 00  	movq	$0x0, 0x40(%rsp)
1400010b9: c7 44 24 38 58 02 00 00     	movl	$0x258, 0x38(%rsp)      # imm = 0x258
1400010c1: c7 44 24 30 20 03 00 00     	movl	$0x320, 0x30(%rsp)      # imm = 0x320
1400010c9: c7 44 24 28 64 00 00 00     	movl	$0x64, 0x28(%rsp)
1400010d1: c7 44 24 20 64 00 00 00     	movl	$0x64, 0x20(%rsp)
1400010d9: 41 b9 00 00 cf 10           	movl	$0x10cf0000, %r9d       # imm = 0x10CF0000
1400010df: 49 89 c8                    	movq	%rcx, %r8
1400010e2: b9 00 00 00 00              	movl	$0x0, %ecx
1400010e7: 48 8b 05 02 40 00 00        	movq	0x4002(%rip), %rax      # 0x1400050f0 <fthunk>
1400010ee: ff d0                       	callq	*%rax
1400010f0: 48 89 45 f0                 	movq	%rax, -0x10(%rbp)
1400010f4: 48 83 7d f0 00              	cmpq	$0x0, -0x10(%rbp)
1400010f9: 75 1d                       	jne	0x140001118 <_start+0xb5>
1400010fb: 48 8d 05 36 0f 00 00        	leaq	0xf36(%rip), %rax       # 0x140002038 <__data_start__+0x38>
140001102: 48 89 c1                    	movq	%rax, %rcx
140001105: e8 f6 fe ff ff              	callq	0x140001000 <print_out>
14000110a: b9 01 00 00 00              	movl	$0x1, %ecx
14000110f: 48 8b 05 a2 3f 00 00        	movq	0x3fa2(%rip), %rax      # 0x1400050b8 <fthunk>
140001116: ff d0                       	callq	*%rax
140001118: 48 8d 05 41 0f 00 00        	leaq	0xf41(%rip), %rax       # 0x140002060 <__data_start__+0x60>
14000111f: 48 89 c1                    	movq	%rax, %rcx
140001122: e8 d9 fe ff ff              	callq	0x140001000 <print_out>
140001127: b9 20 00 00 00              	movl	$0x20, %ecx
14000112c: e8 6f 02 00 00              	callq	0x1400013a0 <Direct3DCreate9>
140001131: 48 89 45 e8                 	movq	%rax, -0x18(%rbp)
140001135: 48 83 7d e8 00              	cmpq	$0x0, -0x18(%rbp)
14000113a: 75 1d                       	jne	0x140001159 <_start+0xf6>
14000113c: 48 8d 05 45 0f 00 00        	leaq	0xf45(%rip), %rax       # 0x140002088 <__data_start__+0x88>
140001143: 48 89 c1                    	movq	%rax, %rcx
140001146: e8 b5 fe ff ff              	callq	0x140001000 <print_out>
14000114b: b9 01 00 00 00              	movl	$0x1, %ecx
140001150: 48 8b 05 61 3f 00 00        	movq	0x3f61(%rip), %rax      # 0x1400050b8 <fthunk>
140001157: ff d0                       	callq	*%rax
140001159: 48 8d 45 90                 	leaq	-0x70(%rbp), %rax
14000115d: 48 89 45 e0                 	movq	%rax, -0x20(%rbp)
140001161: c7 45 fc 00 00 00 00        	movl	$0x0, -0x4(%rbp)
140001168: eb 14                       	jmp	0x14000117e <_start+0x11b>
14000116a: 8b 45 fc                    	movl	-0x4(%rbp), %eax
14000116d: 48 63 d0                    	movslq	%eax, %rdx
140001170: 48 8b 45 e0                 	movq	-0x20(%rbp), %rax
140001174: 48 01 d0                    	addq	%rdx, %rax
140001177: c6 00 00                    	movb	$0x0, (%rax)
14000117a: 83 45 fc 01                 	addl	$0x1, -0x4(%rbp)
14000117e: 8b 45 fc                    	movl	-0x4(%rbp), %eax
140001181: 83 f8 3f                    	cmpl	$0x3f, %eax
140001184: 76 e4                       	jbe	0x14000116a <_start+0x107>
140001186: c7 45 b8 01 00 00 00        	movl	$0x1, -0x48(%rbp)
14000118d: c7 45 a8 01 00 00 00        	movl	$0x1, -0x58(%rbp)
140001194: 48 8b 45 f0                 	movq	-0x10(%rbp), %rax
140001198: 48 89 45 b0                 	movq	%rax, -0x50(%rbp)
14000119c: 48 8d 05 0d 0f 00 00        	leaq	0xf0d(%rip), %rax       # 0x1400020b0 <__data_start__+0xb0>
1400011a3: 48 89 c1                    	movq	%rax, %rcx
1400011a6: e8 55 fe ff ff              	callq	0x140001000 <print_out>
1400011ab: 48 c7 45 88 00 00 00 00     	movq	$0x0, -0x78(%rbp)
1400011b3: 48 8b 45 e8                 	movq	-0x18(%rbp), %rax
1400011b7: 48 8b 00                    	movq	(%rax), %rax
1400011ba: 4c 8b 90 80 00 00 00        	movq	0x80(%rax), %r10
1400011c1: 48 8b 4d f0                 	movq	-0x10(%rbp), %rcx
1400011c5: 48 8b 45 e8                 	movq	-0x18(%rbp), %rax
1400011c9: 48 8d 55 88                 	leaq	-0x78(%rbp), %rdx
1400011cd: 48 89 54 24 30              	movq	%rdx, 0x30(%rsp)
1400011d2: 48 8d 55 90                 	leaq	-0x70(%rbp), %rdx
1400011d6: 48 89 54 24 28              	movq	%rdx, 0x28(%rsp)
1400011db: c7 44 24 20 20 00 00 00     	movl	$0x20, 0x20(%rsp)
1400011e3: 49 89 c9                    	movq	%rcx, %r9
1400011e6: 41 b8 01 00 00 00           	movl	$0x1, %r8d
1400011ec: ba 00 00 00 00              	movl	$0x0, %edx
1400011f1: 48 89 c1                    	movq	%rax, %rcx
1400011f4: 41 ff d2                    	callq	*%r10
1400011f7: 89 45 dc                    	movl	%eax, -0x24(%rbp)
1400011fa: 83 7d dc 00                 	cmpl	$0x0, -0x24(%rbp)
1400011fe: 78 09                       	js	0x140001209 <_start+0x1a6>
140001200: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
140001204: 48 85 c0                    	testq	%rax, %rax
140001207: 75 1d                       	jne	0x140001226 <_start+0x1c3>
140001209: 48 8d 05 d0 0e 00 00        	leaq	0xed0(%rip), %rax       # 0x1400020e0 <__data_start__+0xe0>
140001210: 48 89 c1                    	movq	%rax, %rcx
140001213: e8 e8 fd ff ff              	callq	0x140001000 <print_out>
140001218: b9 01 00 00 00              	movl	$0x1, %ecx
14000121d: 48 8b 05 94 3e 00 00        	movq	0x3e94(%rip), %rax      # 0x1400050b8 <fthunk>
140001224: ff d0                       	callq	*%rax
140001226: 48 8d 05 db 0e 00 00        	leaq	0xedb(%rip), %rax       # 0x140002108 <__data_start__+0x108>
14000122d: 48 89 c1                    	movq	%rax, %rcx
140001230: e8 cb fd ff ff              	callq	0x140001000 <print_out>
140001235: c7 45 f8 00 00 00 00        	movl	$0x0, -0x8(%rbp)
14000123c: e9 b9 00 00 00              	jmp	0x1400012fa <_start+0x297>
140001241: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
140001245: 48 8b 00                    	movq	(%rax), %rax
140001248: 4c 8b 90 58 01 00 00        	movq	0x158(%rax), %r10
14000124f: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
140001253: c7 44 24 30 00 00 00 00     	movl	$0x0, 0x30(%rsp)
14000125b: f3 0f 10 05 f9 0e 00 00     	movss	0xef9(%rip), %xmm0      # 0x14000215c <__data_start__+0x15c>
140001263: f3 0f 11 44 24 28           	movss	%xmm0, 0x28(%rsp)
140001269: c7 44 24 20 ff 00 00 ff     	movl	$0xff0000ff, 0x20(%rsp) # imm = 0xFF0000FF
140001271: 41 b9 01 00 00 00           	movl	$0x1, %r9d
140001277: 41 b8 00 00 00 00           	movl	$0x0, %r8d
14000127d: ba 00 00 00 00              	movl	$0x0, %edx
140001282: 48 89 c1                    	movq	%rax, %rcx
140001285: 41 ff d2                    	callq	*%r10
140001288: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
14000128c: 48 8b 00                    	movq	(%rax), %rax
14000128f: 48 8b 90 48 01 00 00        	movq	0x148(%rax), %rdx
140001296: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
14000129a: 48 89 c1                    	movq	%rax, %rcx
14000129d: ff d2                       	callq	*%rdx
14000129f: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
1400012a3: 48 8b 00                    	movq	(%rax), %rax
1400012a6: 48 8b 90 50 01 00 00        	movq	0x150(%rax), %rdx
1400012ad: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
1400012b1: 48 89 c1                    	movq	%rax, %rcx
1400012b4: ff d2                       	callq	*%rdx
1400012b6: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
1400012ba: 48 8b 00                    	movq	(%rax), %rax
1400012bd: 4c 8b 90 88 00 00 00        	movq	0x88(%rax), %r10
1400012c4: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
1400012c8: 48 c7 44 24 20 00 00 00 00  	movq	$0x0, 0x20(%rsp)
1400012d1: 41 b9 00 00 00 00           	movl	$0x0, %r9d
1400012d7: 41 b8 00 00 00 00           	movl	$0x0, %r8d
1400012dd: ba 00 00 00 00              	movl	$0x0, %edx
1400012e2: 48 89 c1                    	movq	%rax, %rcx
1400012e5: 41 ff d2                    	callq	*%r10
1400012e8: b9 10 00 00 00              	movl	$0x10, %ecx
1400012ed: 48 8b 05 dc 3d 00 00        	movq	0x3ddc(%rip), %rax      # 0x1400050d0 <__imp_Sleep>
1400012f4: ff d0                       	callq	*%rax
1400012f6: 83 45 f8 01                 	addl	$0x1, -0x8(%rbp)
1400012fa: 83 7d f8 3b                 	cmpl	$0x3b, -0x8(%rbp)
1400012fe: 0f 8e 3d ff ff ff           	jle	0x140001241 <_start+0x1de>
140001304: 48 8d 05 21 0e 00 00        	leaq	0xe21(%rip), %rax       # 0x14000212c <__data_start__+0x12c>
14000130b: 48 89 c1                    	movq	%rax, %rcx
14000130e: e8 ed fc ff ff              	callq	0x140001000 <print_out>
140001313: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
140001317: 48 8b 00                    	movq	(%rax), %rax
14000131a: 48 8b 50 10                 	movq	0x10(%rax), %rdx
14000131e: 48 8b 45 88                 	movq	-0x78(%rbp), %rax
140001322: 48 89 c1                    	movq	%rax, %rcx
140001325: ff d2                       	callq	*%rdx
140001327: 48 8b 45 e8                 	movq	-0x18(%rbp), %rax
14000132b: 48 8b 00                    	movq	(%rax), %rax
14000132e: 48 8b 50 10                 	movq	0x10(%rax), %rdx
140001332: 48 8b 45 e8                 	movq	-0x18(%rbp), %rax
140001336: 48 89 c1                    	movq	%rax, %rcx
140001339: ff d2                       	callq	*%rdx
14000133b: 48 8d 05 05 0e 00 00        	leaq	0xe05(%rip), %rax       # 0x140002147 <__data_start__+0x147>
140001342: 48 89 c1                    	movq	%rax, %rcx
140001345: e8 b6 fc ff ff              	callq	0x140001000 <print_out>
14000134a: b9 00 00 00 00              	movl	$0x0, %ecx
14000134f: 48 8b 05 62 3d 00 00        	movq	0x3d62(%rip), %rax      # 0x1400050b8 <fthunk>
140001356: ff d0                       	callq	*%rax
140001358: 90                          	nop
140001359: 90                          	nop
14000135a: 90                          	nop
14000135b: 90                          	nop
14000135c: 90                          	nop
14000135d: 90                          	nop
14000135e: 90                          	nop
14000135f: 90                          	nop

0000000140001360 <lstrlenA>:
140001360: ff 25 7a 3d 00 00           	jmpq	*0x3d7a(%rip)           # 0x1400050e0 <__imp_lstrlenA>
140001366: 90                          	nop
140001367: 90                          	nop

0000000140001368 <WriteFile>:
140001368: ff 25 6a 3d 00 00           	jmpq	*0x3d6a(%rip)           # 0x1400050d8 <__imp_WriteFile>
14000136e: 90                          	nop
14000136f: 90                          	nop

0000000140001370 <Sleep>:
140001370: ff 25 5a 3d 00 00           	jmpq	*0x3d5a(%rip)           # 0x1400050d0 <__imp_Sleep>
140001376: 90                          	nop
140001377: 90                          	nop

0000000140001378 <GetStdHandle>:
140001378: ff 25 4a 3d 00 00           	jmpq	*0x3d4a(%rip)           # 0x1400050c8 <__imp_GetStdHandle>
14000137e: 90                          	nop
14000137f: 90                          	nop

0000000140001380 <GetModuleHandleA>:
140001380: ff 25 3a 3d 00 00           	jmpq	*0x3d3a(%rip)           # 0x1400050c0 <__imp_GetModuleHandleA>
140001386: 90                          	nop
140001387: 90                          	nop

0000000140001388 <ExitProcess>:
140001388: ff 25 2a 3d 00 00           	jmpq	*0x3d2a(%rip)           # 0x1400050b8 <fthunk>
14000138e: 90                          	nop
14000138f: 90                          	nop

0000000140001390 <CreateWindowExA>:
140001390: ff 25 5a 3d 00 00           	jmpq	*0x3d5a(%rip)           # 0x1400050f0 <fthunk>
140001396: 90                          	nop
140001397: 90                          	nop
140001398: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

00000001400013a0 <Direct3DCreate9>:
1400013a0: ff 25 02 3d 00 00           	jmpq	*0x3d02(%rip)           # 0x1400050a8 <fthunk>
1400013a6: 90                          	nop
1400013a7: 90                          	nop
1400013a8: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

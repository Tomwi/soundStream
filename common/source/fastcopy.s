.global fastCopy
.hidden fastCopy
.type fastCopy, %function


.arm
.align 2

@ r0 = out
@ r1 = in
@ r2 = len (bytes)
/* Simple copy function */
fastCopy:

bics r2, r2, #0xf
bxeq lr

push {r4-r6}

.copyLoop:

ldmia r1!,{r3-r6}
subs r2,#16
stmia r0!,{r3-r6}
bne .copyLoop

pop {r4-r6}
bx lr

addi x1, x0, 10
sw x1, 0(x0)
lw x2, 0(x0)
addi x3, x0, 0
loop:
addi x3, x3, 1
out x3
beq x3, x1, end
addi x4, x0, 4
jalr x0, x4, 0
end:
halt

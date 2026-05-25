// 8-bit Interrupt Handling Unit
// Handles interrupts for the 8-bit RISC-V processor
module interrupt_handling_unit (
    input clk,
    input rst,
    // Interrupt request lines
    input timer_interrupt,
    input keyboard_interrupt,
    input syscall_interrupt,
    input external_interrupt,
    // Interrupt acknowledge
    output interrupt_ack,
    // Interrupt vector
    output [7:0] interrupt_vector,
    // Interrupt enable
    input interrupt_enable,
    // Processor status
    output interrupt_pending,
    // Control signals
    input reg_write,
    input mem_write,
    input mem_read,
    // ALU flags
    input zero_flag,
    // Output control
    output take_interrupt,
    output [7:0] next_pc_after_interrupt  // PC to jump to when handling interrupt
);

    // Internal signals
    reg [7:0] current_interrupt_vector;
    reg pending_interrupt;

    // Detect pending interrupts
    always @(*) begin
        pending_interrupt = timer_interrupt | keyboard_interrupt | 
                           syscall_interrupt | external_interrupt;
    end

    // Assign which interrupt is pending
    always @(*) begin
        if (timer_interrupt)
            current_interrupt_vector = 8'h01;  // Timer interrupt vector
        else if (keyboard_interrupt)
            current_interrupt_vector = 8'h02;  // Keyboard interrupt vector
        else if (syscall_interrupt)
            current_interrupt_vector = 8'h03;  // System call interrupt vector
        else if (external_interrupt)
            current_interrupt_vector = 8'h04;  // External interrupt vector
        else
            current_interrupt_vector = 8'h00;  // No interrupt
    end

    // Assign outputs
    assign interrupt_vector = current_interrupt_vector;
    assign interrupt_pending = pending_interrupt;
    assign interrupt_ack = take_interrupt;  // Acknowledge when taking interrupt

    // Determine if we should take the interrupt
    always @(*) begin
        take_interrupt = interrupt_enable & pending_interrupt & !reg_write & !mem_write & !mem_read;
    end

    // PC to jump to when handling interrupt (simplified)
    // In a real implementation, this would be based on the interrupt vector table
    assign next_pc_after_interrupt = {interrupt_vector, 1'b0};  // Simple mapping

endmodule

// Combined interrupt controller
module interrupt_controller (
    input clk,
    input rst,
    // External interrupt sources
    input timer_irq,
    input keyboard_irq,
    input external_irq,
    // System call interrupt (from instruction decoder)
    input syscall_irq,
    // Interrupt enable from CSR
    input mstatus_mie,  // Machine status register, interrupt enable bit
    // Outputs to processor core
    output interrupt_req,
    output [2:0] interrupt_cause,  // Type of interrupt
    output [7:0] interrupt_handler_addr
);
    
    // Internal signals
    wire any_interrupt = timer_irq | keyboard_irq | external_irq | syscall_irq;
    
    // Determine which interrupt has highest priority
    // In order: timer, external, keyboard, syscall
    assign interrupt_cause = timer_irq   ? 3'b001 :  // Timer interrupt
                            external_irq ? 3'b010 :  // External interrupt
                            keyboard_irq ? 3'b011 :  // Keyboard interrupt
                            syscall_irq  ? 3'b100 :  // System call
                            3'b000;                 // No interrupt
    
    // Generate interrupt request
    assign interrupt_req = mstatus_mie & any_interrupt;
    
    // Assign handler address based on interrupt type
    assign interrupt_handler_addr = interrupt_cause == 3'b001 ? 8'h10 :  // Timer handler
                                   interrupt_cause == 3'b010 ? 8'h14 :  // External handler
                                   interrupt_cause == 3'b011 ? 8'h18 :  // Keyboard handler
                                   interrupt_cause == 3'b100 ? 8'h1C :  // System call handler
                                   8'h00;                              // Default

endmodule
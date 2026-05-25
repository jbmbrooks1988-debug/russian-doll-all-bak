// Interrupt Handling Unit for 16-bit RISC-V processor
// Manages hardware and software interrupts with kernel compatibility
module interrupt_handling_unit (
    input clk,
    input rst,
    // Interrupt request lines
    input timer_interrupt,
    input external_interrupt,
    input software_interrupt,
    // Processor control
    input [15:0] mstatus_mie,  // Machine status register - interrupt enable bit
    input [15:0] mepc,         // Machine exception program counter
    input [15:0] mtvec,        // Machine trap vector base address
    output [15:0] mcause,      // Machine cause register
    // Interrupt control
    output interrupt_req,
    output [2:0] interrupt_cause,
    output [15:0] interrupt_handler_addr,
    output take_interrupt
);

    // Internal signals
    reg [2:0] pending_interrupts;
    reg [2:0] active_interrupt;
    
    // Interrupt cause codes
    parameter TIMER_INTERRUPT = 3'b001;
    parameter EXTERNAL_INTERRUPT = 3'b010;
    parameter SOFTWARE_INTERRUPT = 3'b011;
    
    // Detect pending interrupts
    always @(*) begin
        pending_interrupts = 3'b000;
        if (timer_interrupt && mstatus_mie[3])  // Assuming bit 3 is timer interrupt enable
            pending_interrupts[0] = 1;
        if (external_interrupt && mstatus_mie[4])  // Assuming bit 4 is external interrupt enable
            pending_interrupts[1] = 1;
        if (software_interrupt && mstatus_mie[5])  // Assuming bit 5 is software interrupt enable
            pending_interrupts[2] = 1;
    end
    
    // Determine active interrupt (simple priority: timer > external > software)
    always @(*) begin
        if (pending_interrupts[0])  // Timer interrupt
            active_interrupt = TIMER_INTERRUPT;
        else if (pending_interrupts[1])  // External interrupt
            active_interrupt = EXTERNAL_INTERRUPT;
        else if (pending_interrupts[2])  // Software interrupt
            active_interrupt = SOFTWARE_INTERRUPT;
        else
            active_interrupt = 3'b000;  // No interrupt
    end
    
    // Assign outputs
    assign interrupt_req = |pending_interrupts;  // At least one interrupt pending
    assign interrupt_cause = active_interrupt;
    assign take_interrupt = interrupt_req && (mstatus_mie != 16'b0);  // Interrupts enabled
    
    // Calculate interrupt handler address
    // For simplicity, we'll use mtvec as the base address + offset based on interrupt type
    always @(*) begin
        case (active_interrupt)
            TIMER_INTERRUPT: interrupt_handler_addr = mtvec + 16'h0004;  // Timer interrupt handler
            EXTERNAL_INTERRUPT: interrupt_handler_addr = mtvec + 16'h0008;  // External interrupt handler
            SOFTWARE_INTERRUPT: interrupt_handler_addr = mtvec + 16'h000C;  // Software interrupt handler
            default: interrupt_handler_addr = mtvec;  // Default to mtvec
        endcase
    end
    
    // Assign mcause based on active interrupt
    always @(*) begin
        case (active_interrupt)
            TIMER_INTERRUPT: mcause = 16'h80000001;  // ECALL from U-mode (simplified)
            EXTERNAL_INTERRUPT: mcause = 16'h80000003;  // Breakpoint (simplified)
            SOFTWARE_INTERRUPT: mcause = 16'h00000001;  // Instruction address misaligned
            default: mcause = 16'h0000;
        endcase
    end

endmodule
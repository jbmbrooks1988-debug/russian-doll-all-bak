// Testbench for NOT gate implemented using NAND gate
module not_gate_tb;
    reg a;
    wire y;
    
    // Instantiate the NOT gate
    not_gate uut (
        .a(a),
        .y(y)
    );
    
    // Test all possible input combinations
    initial begin
        $display("NOT Gate Testbench");
        $display("Time\ta\tY");
        $monitor("%0t\t%b\t%b", $time, a, y);
        
        // Test case 1: a = 0
        a = 0;
        #10;
        
        // Test case 2: a = 1
        a = 1;
        #10;
        
        // Finish simulation
        $finish;
    end
    
    // Optional: Write VCD file for waveform viewing
    initial begin
        $dumpfile("not_gate_tb.vcd");
        $dumpvars(0, not_gate_tb);
    end
endmodule
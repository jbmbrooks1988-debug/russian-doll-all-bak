// Testbench for XNOR gate implemented using NAND gates
module xnor_gate_tb;
    reg a, b;
    wire y;
    
    // Instantiate the XNOR gate
    xnor_gate uut (
        .a(a),
        .b(b),
        .y(y)
    );
    
    // Test all possible input combinations
    initial begin
        $display("XNOR Gate Testbench");
        $display("Time\ta\tb\tY");
        $monitor("%0t\t%b\t%b\t%b", $time, a, b, y);
        
        // Test case 1: a = 0, b = 0
        a = 0; b = 0;
        #10;
        
        // Test case 2: a = 0, b = 1
        a = 0; b = 1;
        #10;
        
        // Test case 3: a = 1, b = 0
        a = 1; b = 0;
        #10;
        
        // Test case 4: a = 1, b = 1
        a = 1; b = 1;
        #10;
        
        // Finish simulation
        $finish;
    end
    
    // Optional: Write VCD file for waveform viewing
    initial begin
        $dumpfile("xnor_gate_tb.vcd");
        $dumpvars(0, xnor_gate_tb);
    end
endmodule
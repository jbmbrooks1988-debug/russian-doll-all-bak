// Testbench for XOR gate implemented using NAND gates
module xor_gate_tb;
    reg a, b;
    wire y;
    
    // Instantiate the XOR gate
    xor_gate uut (
        .a(a),
        .b(b),
        .y(y)
    );
    
    // Test all possible input combinations
    initial begin
        $display("XOR Gate Testbench");
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
        $dumpfile("xor_gate_tb.vcd");
        $dumpvars(0, xor_gate_tb);
    end
endmodule
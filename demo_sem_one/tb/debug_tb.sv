`timescale 1ns/1ps
module debug_tb;
    localparam int CLK_PERIOD = 20;
    localparam int SPI_HALF_PERIOD = 500; // 1 MHz SCK (safe for 50 MHz oversampling)
    logic clk_50=0, rst_n=0, lab_clk=0;
    logic s_l=0, cin=0, cout=0;
    logic [3:0] input_data=0, msa=0, msb=0, msc=0;
    logic [3:0] reg_a=0, reg_b=0, output_data=0;
    logic [3:0] a_and_b=0, a_or_b=0, a_plus_b=0;
    logic [43:0] probes;
    logic spi_clk=0, spi_cs_n=1, spi_mosi=0, spi_miso, data_ready;
    logic [7:0] rx[0:127];
    logic [43:0] expected_snapshot;
    logic [15:0] expected_sample=0;
    int errors=0;
    string export_dir = "demo_sem_one/tb/generated/run-1";
    bit export_enabled=0;

    always #(CLK_PERIOD/2) clk_50=~clk_50;
    debug_ip debug_inst (.*,.clk(lab_clk));
    spi_slave spi_inst (.*);

    task automatic apply_stimulus(input int n);
        s_l = n[0]; cin = n[1]; cout = n[2];
        input_data = 4'(n+3);
        // Match the zero-extended control inputs in debug_demo.
        msa = {2'b00, 2'(n+1)}; msb = {2'b00, 2'(n+2)};
        msc = {1'b0, 3'(n+3)}; reg_a = 4'(n+4); reg_b = 4'(n+5);
        output_data = 4'(n+6);
        a_and_b = reg_a & reg_b;
        a_or_b = reg_a | reg_b;
        a_plus_b = reg_a + reg_b + cin;
    endtask

    task automatic capture_sample;
        // Hold a realistic pushbutton-style pulse long enough to be sampled.
        lab_clk=1; #1000;
        // Snapshot actually occurs while lab_clk is HIGH.
        wait(data_ready === 1'b1);
        expected_snapshot=probes;
        expected_sample=expected_sample+1'b1;
        lab_clk=0; #200;
    endtask

    task automatic spi_byte(output logic [7:0] read_data);
        read_data=0;
        for (int b=7; b>=0; b--) begin
            spi_mosi=0; #(SPI_HALF_PERIOD);
            spi_clk=1;
            #10; // sample after the rising edge, before falling edge
            read_data[b]=spi_miso;
            #(SPI_HALF_PERIOD-10);
            spi_clk=0;
        end
    endtask

    task automatic transaction(input int byte_count);
        logic [7:0] data_byte;
        spi_cs_n=0; #200;
        for (int i=0; i<byte_count; i++) begin
            spi_byte(data_byte);
            rx[i]=data_byte;
        end
        #200; spi_cs_n=1; #200;
    endtask

    function automatic logic [7:0] signal_width(input int id);
        case(id)
            0,1,2,3: return 8'd1;
            5,6: return 8'd2;
            7: return 8'd3;
            default: return 8'd4;
        endcase
    endfunction
    function automatic logic [7:0] expected_value(input int id);
        case(id)
            0: return {7'b0,expected_snapshot[0]};
            1: return {7'b0,expected_snapshot[1]};
            2: return {7'b0,expected_snapshot[2]};
            3: return {7'b0,expected_snapshot[3]};
            default: return 8'((expected_snapshot >> (4+(id-4)*4)) & 44'hF);
        endcase
    endfunction

    task automatic check_packet;
        int offset;
        logic [7:0] xor_sum;
        if(rx[0] !== 8'hA5) begin $error("start marker"); errors++; end
        if(rx[1] !== 8'h01) begin $error("protocol version"); errors++; end
        if({rx[3],rx[2]} !== expected_sample) begin
            $error("sample count expected=%0d got=%0d", expected_sample,{rx[3],rx[2]}); errors++;
        end
        offset=4;
        for(int id=0; id<14; id++) begin
            if(rx[offset] !== 8'(id)) begin
                $error("record %0d ID expected=%0d got=%0h",id,id,rx[offset]); errors++;
            end
            if(rx[offset+1] !== signal_width(id)) begin
                $error("record %0d width",id); errors++;
            end
            if(rx[offset+2] !== expected_value(id)) begin
                $error("record %0d value expected=%02h got=%02h",id,expected_value(id),rx[offset+2]); errors++;
            end
            offset+=3;
        end
        if(offset != 46) begin $error("record offset error"); errors++; end
        for(int i=46;i<127;i++) if(rx[i] !== 0) begin
            $error("nonzero padding byte %0d",i); errors++;
        end
        xor_sum=0;
        for(int i=0;i<127;i++) xor_sum ^= rx[i];
        if(rx[127] !== xor_sum) begin
            $error("checksum expected=%02h got=%02h", xor_sum,rx[127]); errors++;
        end
    endtask

    // Export observed SPI bytes, never a separately reconstructed packet.
    // Expectations come from the snapshot saved before pending inputs change.
    task automatic export_packet(input int test_number);
        int fd;
        string path;
        path=$sformatf("%s/sample_%0d.txt", export_dir, test_number);
        fd=$fopen(path, "w");
        if(fd==0) $fatal(1,"Cannot open %s; create the export directory first",path);
        $fdisplay(fd,"PLD_SPI_FIXTURE_V1");
        $fdisplay(fd,"sample %0d",expected_sample);
        $fwrite(fd,"bytes");
        for(int i=0;i<128;i++) $fwrite(fd," %02h",rx[i]);
        $fwrite(fd,"\n");
        for(int id=0;id<14;id++)
            $fdisplay(fd,"signal %0d %0d %0d",id,signal_width(id),expected_value(id));
        $fclose(fd);
        $display("EXPORTED: %s",path);
    endtask


    initial begin : test_sequence
        export_enabled=$value$plusargs("EXPORT_DIR=%s",export_dir);
        #200; rst_n=1; #200;
        for(int t=0;t<3;t++) begin
            apply_stimulus(t);
            capture_sample();
            // Confirm pending sample is NOT replaced by a new button event.
            apply_stimulus(t+9);
            lab_clk=1; #1000; lab_clk=0; #200; // ignored while sample pending
            if(t==1) begin
                $display("TEST %0d: abort after 8 bytes, then retry", t+1);
                transaction(8);
                if(data_ready !== 1'b1) begin $error("aborted frame lost ready");errors++;end
            end
            $display("TEST %0d: full 128-byte SPI read", t+1);
            transaction(128);
            check_packet();
            if(data_ready !== 1'b0) begin $error("full frame did not clear ready");errors++;end
            // Any regression failure prevents subsequent fixture exports.
            if(export_enabled && errors==0) export_packet(t+1);
            #1000;
        end
        if(errors==0) $display("PASS: all 3 samples, interrupted retry, and checksums");
        else $fatal(1,"FAIL: %0d errors",errors);
        $finish;
    end
    initial begin
        #10000000;
        $fatal(1,"TEST TIMEOUT");
    end
endmodule

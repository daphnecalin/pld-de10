`timescale 1ns/1ps
module debug_tb;
    localparam int CLK_PERIOD = 20;
    localparam int SPI_HALF_PERIOD = 500; // 1 MHz SCK for 50 MHz oversampling

    localparam logic [2:0] STATUS_OK = 3'd1;
    localparam logic [2:0] STATUS_BAD_FRAME = 3'd2;
    localparam logic [2:0] STATUS_BAD_ID = 3'd3;
    localparam logic [2:0] STATUS_BAD_WIDTH = 3'd4;
    localparam logic [2:0] STATUS_BAD_VALUE = 3'd5;
    localparam logic [2:0] STATUS_BAD_OPCODE = 3'd6;

    logic clk_50=0, rst_n=0, lab_clk=0;
    logic s_l_pin=0, cin_pin=0;
    logic [3:0] input_data_pin=0;
    logic [1:0] msa_pin=0, msb_pin=0;
    logic [2:0] msc_pin=0;

    logic s_l, cin;
    logic [3:0] input_data;
    logic [1:0] msa, msb;
    logic [2:0] msc;
    logic cout=0;
    logic [3:0] reg_a=0, reg_b=0, output_data=0;
    logic [3:0] a_and_b=0, a_or_b=0, a_plus_b=0;
    logic [43:0] probes;

    logic spi_clk=0, spi_cs_n=1, spi_mosi=0, spi_miso, data_ready;
    logic command_done;
    logic [2:0] command_status;
    logic [43:0] control_values;
    logic [13:0] control_enable;
    logic [7:0] control_update_gray;

    logic [7:0] rx[0:127];
    logic [43:0] expected_snapshot;
    logic [15:0] expected_sample=0;
    logic [7:0] command_sequence=0;
    int errors=0;
    string export_dir = "uP/src/test";
    bit export_enabled=0;

    assign s_l = control_enable[1] ? control_values[1] : s_l_pin;
    assign cin = control_enable[2] ? control_values[2] : cin_pin;
    assign input_data = control_enable[4] ? control_values[7:4] : input_data_pin;
    assign msa = control_enable[5] ? control_values[9:8] : msa_pin;
    assign msb = control_enable[6] ? control_values[13:12] : msb_pin;
    assign msc = control_enable[7] ? control_values[18:16] : msc_pin;

    always #(CLK_PERIOD/2) clk_50=~clk_50;

    debug_ip debug_inst (
        .clk(lab_clk), .s_l(s_l), .cin(cin), .cout(cout),
        .input_data(input_data), .msa({2'b00, msa}), .msb({2'b00, msb}),
        .msc({1'b0, msc}), .reg_a(reg_a), .reg_b(reg_b),
        .output_data(output_data), .a_and_b(a_and_b), .a_or_b(a_or_b),
        .a_plus_b(a_plus_b), .probes(probes)
    );

    spi_slave spi_inst (
        .clk_50(clk_50), .rst_n(rst_n), .lab_clk(lab_clk), .probes(probes),
        .spi_clk(spi_clk), .spi_cs_n(spi_cs_n), .spi_mosi(spi_mosi),
        .spi_miso(spi_miso), .data_ready(data_ready),
        .command_done(command_done), .command_status(command_status),
        .control_values(control_values), .control_enable(control_enable),
        .control_update_gray(control_update_gray)
    );

    task automatic apply_stimulus(input int n);
        s_l_pin = n[0];
        cin_pin = n[1];
        input_data_pin = 4'(n+3);
        msa_pin = 2'(n+1);
        msb_pin = 2'(n+2);
        msc_pin = 3'(n+3);
        cout = n[2];
        reg_a = 4'(n+4);
        reg_b = 4'(n+5);
        output_data = 4'(n+6);
        a_and_b = reg_a & reg_b;
        a_or_b = reg_a | reg_b;
        a_plus_b = reg_a + reg_b + cin;
    endtask

    task automatic capture_sample;
        lab_clk=1; #1000;
        wait(data_ready === 1'b1);
        expected_snapshot=probes;
        expected_sample=expected_sample+1'b1;
        lab_clk=0; #200;
    endtask

    task automatic spi_transfer_byte(input logic [7:0] tx_data, output logic [7:0] rx_data);
        rx_data=0;
        for (int b=7; b>=0; b--) begin
            spi_mosi=tx_data[b];
            #(SPI_HALF_PERIOD);
            spi_clk=1;
            #10;
            rx_data[b]=spi_miso;
            #(SPI_HALF_PERIOD-10);
            spi_clk=0;
        end
    endtask

    task automatic transaction(input int byte_count);
        logic [7:0] data_byte;
        spi_cs_n=0; #200;
        for (int i=0; i<byte_count; i++) begin
            spi_transfer_byte(8'h00, data_byte);
            if (i < 128) rx[i]=data_byte;
        end
        #200; spi_cs_n=1; #200;
    endtask

    task automatic write_command(
        input logic [7:0] opcode,
        input logic [7:0] id,
        input logic [7:0] width,
        input logic [31:0] value,
        input bit corrupt_checksum,
        input logic [2:0] expected_status
    );
        logic [7:0] command_frame[0:10];
        logic [7:0] ignored;
        logic [7:0] checksum;
        command_frame[0] = 8'hC3;
        command_frame[1] = 8'h01;
        command_frame[2] = opcode;
        command_frame[3] = command_sequence;
        command_frame[4] = id;
        command_frame[5] = width;
        command_frame[6] = value[7:0];
        command_frame[7] = value[15:8];
        command_frame[8] = value[23:16];
        command_frame[9] = value[31:24];
        checksum=0;
        for(int i=0; i<10; i++) checksum ^= command_frame[i];
        command_frame[10] = corrupt_checksum ? (checksum ^ 8'h01) : checksum;
        command_sequence++;

        spi_cs_n=0; #200;
        for(int i=0; i<11; i++)
            spi_transfer_byte(command_frame[i], ignored);
        #200; spi_cs_n=1; #200;

        if(command_done !== 1'b1) begin
            $error("command_done was not asserted for id=%0d", id);
            errors++;
        end
        if(command_status !== expected_status) begin
            $error("command status for id=%0d expected=%0d got=%0d",
                id, expected_status, command_status);
            errors++;
        end
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

    task automatic check_effective_controls;
        if ({msc, msb, msa, input_data, cin, s_l} !==
        {3'd5, 2'd1, 2'd2, 4'hA, 1'b1, 1'b1}) begin
            $error("effective inputs do not match written control values");
            errors++;
        end
    endtask

    initial begin : test_sequence
        export_enabled=$value$plusargs("EXPORT_DIR=%s",export_dir);
        #200; rst_n=1; #200;

        // Keep the established read-path regression and packet fixture export.
        for(int t=0;t<3;t++) begin
            apply_stimulus(t);
            capture_sample();
            apply_stimulus(t+9);
            lab_clk=1; #1000; lab_clk=0; #200;
            if(t==1) begin
                $display("TEST: abort read, then retry same sample");
                transaction(8);
                if(data_ready !== 1'b1) begin $error("aborted frame lost ready"); errors++; end
            end
            transaction(128);
            check_packet();
            if(data_ready !== 1'b0) begin $error("full frame did not clear ready"); errors++; end
            if(export_enabled && errors==0) export_packet(t+1);
            #1000;
        end

        $display("TEST: set all supported RALU input overrides");
        write_command(8'h01, 8'd1, 8'd1, 32'd1, 0, STATUS_OK);
        write_command(8'h01, 8'd2, 8'd1, 32'd1, 0, STATUS_OK);
        write_command(8'h01, 8'd4, 8'd4, 32'hA, 0, STATUS_OK);
        write_command(8'h01, 8'd5, 8'd2, 32'd2, 0, STATUS_OK);
        write_command(8'h01, 8'd6, 8'd2, 32'd1, 0, STATUS_OK);
        write_command(8'h01, 8'd7, 8'd3, 32'd5, 0, STATUS_OK);
        check_effective_controls();
        if(control_enable[13:0] !== 14'h00F6) begin
            $error("override enable bits do not match accepted writes"); errors++;
        end

        apply_stimulus(3);
        capture_sample();
        transaction(128);
        check_packet();
        if(input_data !== 4'hA || {msa,msb,msc} !== {2'd2,2'd1,3'd5}) begin
            $error("telemetry sample did not include written inputs"); errors++;
        end
        if(data_ready !== 1'b0) begin $error("sample after writes was not consumed"); errors++; end

        $display("TEST: reject bad checksum, ID, width, value, and opcode");
        write_command(8'h01, 8'd4, 8'd4, 32'h3, 1, STATUS_BAD_FRAME);
        write_command(8'h01, 8'd8, 8'd4, 32'h3, 0, STATUS_BAD_ID);
        write_command(8'h01, 8'd5, 8'd4, 32'h1, 0, STATUS_BAD_WIDTH);
        write_command(8'h01, 8'd4, 8'd4, 32'h10, 0, STATUS_BAD_VALUE);
        write_command(8'h7F, 8'd4, 8'd4, 32'h3, 0, STATUS_BAD_OPCODE);
        if(control_values[7:4] !== 4'hA || control_enable[4] !== 1'b1) begin
            $error("rejected commands changed the active input override"); errors++;
        end

        $display("TEST: write while sample is pending; preserve the old sample");
        apply_stimulus(4);
        capture_sample();
        write_command(8'h01, 8'd4, 8'd4, 32'h3, 0, STATUS_OK);
        if(data_ready !== 1'b1) begin $error("write consumed a pending sample"); errors++; end
        transaction(128);
        check_packet();
        if(expected_value(4) !== 8'hA) begin
            $error("pending sample was modified by a later write"); errors++;
        end
        if(data_ready !== 1'b0) begin $error("pending sample read did not clear ready"); errors++; end

        $display("TEST: clear override and use physical input again");
        write_command(8'h02, 8'd4, 8'd4, 32'd0, 0, STATUS_OK);
        if(control_enable[4] !== 1'b0 || input_data !== input_data_pin) begin
            $error("clear override did not return control to physical input"); errors++;
        end
        apply_stimulus(5);
        capture_sample();
        transaction(128);
        check_packet();

        if(errors==0) $display("PASS: telemetry reads, retries, writes, validation, pending samples, and clear");
        else $fatal(1,"FAIL: %0d errors",errors);
        $finish;
    end

    initial begin
        #50000000;
        $fatal(1,"TEST TIMEOUT");
    end
endmodule

`timescale 1ns / 1ps

module debug_tb;

    localparam int CLK_PERIOD = 20;
    localparam int SPI_HALF_PERIOD = 500;

    logic clk_50 = 0;
    logic rst_n = 0;

    logic lab_clk = 0;

    logic s_l;
    logic cin;
    logic cout;

    logic [3:0] input_data;
    logic [3:0] msa;
    logic [3:0] msb;
    logic [3:0] msc;
    logic [3:0] reg_a;
    logic [3:0] reg_b;
    logic [3:0] output_data;
    logic [3:0] a_and_b;
    logic [3:0] a_or_b;
    logic [3:0] a_plus_b;

    logic [43:0] probes;

    logic spi_clk = 0;
    logic spi_cs_n = 1;
    logic spi_mosi = 0;
    logic spi_miso;
    logic data_ready;

    logic [7:0] rx_buffer [0:127];

    logic [43:0] expected_probes;
    logic [15:0] expected_cycle;

    int errors = 0;

    // DE10-Lite 50 MHz oscillator
    always #(CLK_PERIOD / 2) clk_50 = ~clk_50;

    // Signal mapping IP
    debug_ip debug_inst (
        .clk(lab_clk),
        .s_l(s_l),
        .cin(cin),
        .cout(cout),

        .input_data(input_data),
        .msa(msa),
        .msb(msb),
        .msc(msc),
        .reg_a(reg_a),
        .reg_b(reg_b),

        .output_data(output_data),
        .a_and_b(a_and_b),
        .a_or_b(a_or_b),
        .a_plus_b(a_plus_b),

        .probes(probes)
    );

    // FPGA SPI slave
    spi_slave spi_inst (
        .clk_50(clk_50),
        .rst_n(rst_n),
        .lab_clk(lab_clk),

        .probes(probes),

        .spi_clk(spi_clk),
        .spi_cs_n(spi_cs_n),
        .spi_mosi(spi_mosi),

        .spi_miso(spi_miso),
        .data_ready(data_ready)
    );

    // Simulate one push-button clock pulse
    task automatic press_button();
        begin
            $display("[%0t] Student presses clock button", $time);

            lab_clk = 1'b1;

            #1000;

            lab_clk = 1'b0;

            #1000;
        end
    endtask

    // Simulate the RP2040 transmitting and receiving one SPI byte
    // SPI mode 0: CPOL = 0, CPHA = 0
    task automatic spi_transfer_byte(
        input logic [7:0] tx_data,
        output logic [7:0] rx_data
    );
        begin
            rx_data = 8'h00;

            for (int i = 7; i >= 0; i--) begin
                spi_mosi = tx_data[i];

                #(SPI_HALF_PERIOD);

                spi_clk = 1'b1;

                // RP2040 samples MISO on rising edge
                rx_data[i] = spi_miso;

                #(SPI_HALF_PERIOD);

                spi_clk = 1'b0;
            end
        end
    endtask

    // Simulate the RP2040 receiving a complete 128-byte packet
    task automatic spi_read_packet();
        logic [7:0] rx_data;

        begin
            $display("[%0t] RP2040 starts SPI transfer", $time);

            spi_clk = 1'b0;
            spi_cs_n = 1'b0;

            // Give FPGA time to detect CS falling
            #200;

            for (int i = 0; i < 128; i++) begin
                spi_transfer_byte(8'h00, rx_data);
                rx_buffer[i] = rx_data;
            end

            #200;

            spi_cs_n = 1'b1;

            // Give FPGA time to complete transaction
            #200;

            $display("[%0t] RP2040 received 128 bytes", $time);
        end
    endtask

    // Verify the complete received packet
    task automatic check_packet();
        logic [7:0] expected_checksum;
        logic [7:0] received_checksum;
        logic [15:0] received_cycle;
        int signal_id;

        begin
            expected_checksum = 8'h00;

            $display("");
            $display("========================================");
            $display("Checking received SPI packet");
            $display("========================================");

            // Verify header
            if (rx_buffer[0] !== 8'hA5) begin
                $error("Invalid start byte: %02h", rx_buffer[0]);
                errors++;
            end

            if (rx_buffer[1] !== 8'h01) begin
                $error("Invalid protocol version: %02h", rx_buffer[1]);
                errors++;
            end

            // Verify cycle counter
            received_cycle = {rx_buffer[3], rx_buffer[2]};

            if (received_cycle !== expected_cycle) begin
                $error("Cycle mismatch: expected %0d, received %0d",
                       expected_cycle, received_cycle);
                errors++;
            end

            $display("Cycle: %0d", received_cycle);

            // Verify all 44 signal IDs and values
            for (int i = 0; i < 44; i++) begin
                signal_id = rx_buffer[4 + (2 * i)];

                if (signal_id != i) begin
                    $error("Signal ID mismatch at index %0d: got %0d",
                           i, signal_id);
                    errors++;
                end

                if (rx_buffer[5 + (2 * i)] !== {7'b0, expected_probes[i]}) begin
                    $error("Signal %0d mismatch: expected %b, got %02h",
                           i, expected_probes[i],
                           rx_buffer[5 + (2 * i)]);
                    errors++;
                end

                $display("Signal ID %02d = %b",
                         signal_id, rx_buffer[5 + (2 * i)][0]);
            end

            // Verify reserved bytes
            for (int i = 92; i < 127; i++) begin
                if (rx_buffer[i] !== 8'h00) begin
                    $error("Reserved byte %0d is not zero", i);
                    errors++;
                end
            end

            // Verify XOR checksum
            for (int i = 0; i < 127; i++) begin
                expected_checksum ^= rx_buffer[i];
            end

            received_checksum = rx_buffer[127];

            if (received_checksum !== expected_checksum) begin
                $error("Checksum mismatch: expected %02h, got %02h",
                       expected_checksum, received_checksum);
                errors++;
            end

            $display("Checksum: %02h", received_checksum);
            $display("========================================");
        end
    endtask

    // Assign test values to the RALU signals
    task automatic set_test_signals(input int test_number);
        begin
            s_l = test_number[0];
            cin = test_number[1];
            cout = test_number[2];

            input_data = 4'(test_number + 3);
            msa = 4'(test_number);
            msb = 4'(test_number + 1);
            msc = 4'(test_number + 2);

            reg_a = 4'(test_number + 4);
            reg_b = 4'(test_number + 5);

            output_data = 4'(test_number + 6);
            a_and_b = reg_a & reg_b;
            a_or_b = reg_a | reg_b;
            a_plus_b = reg_a + reg_b + cin;
        end
    endtask

    initial begin
        // Initialize signals
        s_l = 0;
        cin = 0;
        cout = 0;

        input_data = 0;
        msa = 0;
        msb = 0;
        msc = 0;

        reg_a = 0;
        reg_b = 0;
        output_data = 0;
        a_and_b = 0;
        a_or_b = 0;
        a_plus_b = 0;

        expected_cycle = 0;

        spi_clk  = 0;
        spi_cs_n = 1;
        spi_mosi = 0;

        // Reset
        #200;
        rst_n = 1;

        #200;

        // Run three example button presses
        for (int test_number = 0; test_number < 3; test_number++) begin

            $display("");
            $display("******** TEST %0d ********", test_number + 1);

            set_test_signals(test_number);

            press_button();

            // Wait for FPGA to capture snapshot
            wait (data_ready == 1'b1);

            expected_cycle++;

            expected_probes = probes;
            expected_probes[0] = 1'b1;

            // Simulate MCU SPI master
            spi_read_packet();

            // Check received data
            check_packet();

            // Wait until FPGA releases data_ready
            wait (data_ready == 1'b0);

            #1000;
        end

        $display("");
        $display("========================================");

        if (errors == 0) begin
            $display("ALL TESTS PASSED");
        end
        else begin
            $display("TEST FAILED: %0d errors", errors);
        end

        $display("========================================");

        $finish;
    end

endmodule

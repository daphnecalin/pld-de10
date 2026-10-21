module spi_slave (
    input logic clk_50,
    input logic rst_n,
    input logic lab_clk,

    input logic [43:0] probes,

    input logic spi_clk,
    input logic spi_cs_n,
    input logic spi_mosi,

    output logic spi_miso,
    output logic data_ready
);
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    typedef enum logic [1:0] {
        idle, wait_settle, ready, transfer
    } state_type;

    state_type current_state;
    state_type next_state;
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    logic [7:0] checksum;
    logic [7:0] tx_byte;
    logic [7:0] next_tx_byte;

    logic [15:0] cycle_count;
    logic [2:0] settle_count;
    logic [43:0] debug_data;

    logic [6:0] byte_index;
    logic [2:0] bit_index;
    logic frame_complete_flag;

    logic [2:0] lab_clk_sync;
    logic [2:0] spi_clk_sync;
    logic [2:0] spi_cs_n_sync;

    logic lab_clock_rising_flag;
    logic spi_clk_rising_flag;
    logic spi_clk_falling_flag;
    logic spi_cs_n_rising_flag;
    logic spi_cs_n_falling_flag;
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Synchronize asynchronous inputs to the 50 MHz clock
    always_ff @(posedge clk_50 or negedge rst_n) begin
        if (!rst_n) begin
            lab_clk_sync <= '0;
            spi_clk_sync <= '0;
            spi_cs_n_sync <= '1;
        end else begin
            lab_clk_sync <= {lab_clk_sync[1:0], lab_clk};
            spi_clk_sync <= {spi_clk_sync[1:0], spi_clk};
            spi_cs_n_sync <= {spi_cs_n_sync[1:0], spi_cs_n};
        end
    end

    assign lab_clock_rising_flag = lab_clk_sync[1] && !lab_clk_sync[2];

    assign spi_clk_rising_flag = spi_clk_sync[1] && !spi_clk_sync[2];
    assign spi_clk_falling_flag = !spi_clk_sync[1] && spi_clk_sync[2];

    assign spi_cs_n_rising_flag = spi_cs_n_sync[1] && !spi_cs_n_sync[2];
    assign spi_cs_n_falling_flag = !spi_cs_n_sync[1] && spi_cs_n_sync[2];
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Calculate checksum
    always_comb begin
        checksum = 8'hA5 ^ 8'h01;
        checksum = checksum ^ cycle_count[7:0];
        checksum = checksum ^ cycle_count[15:8];

        for (int i = 0; i < 44; i++) begin
            checksum = checksum ^ 8'(i);
            checksum = checksum ^ {7'b0, debug_data[i]};
        end
    end
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Packet generation
    function automatic logic [7:0] packet_generation (
        input logic [6:0] index
    );
        logic [7:0] result;
        logic [6:0] signal_id;

        result = 8'h00;
        signal_id = '0;

        if (index == 7'd0) begin
            // Start byte
            result = 8'hA5;
        end else if (index == 7'd1) begin
            // Protocol version
            result = 8'h01;
        end else if (index == 7'd2) begin
            // Cycle count low byte
            result = cycle_count[7:0];
        end else if (index == 7'd3) begin
            // Cycle count high byte
            result = cycle_count[15:8];
        end else if (index >= 7'd4 && index < 7'd92) begin
            // Each signal contains two bytes: {id, value}
            signal_id = (index - 7'd4) >> 1;

            if (index[0] == 1'b0) begin
                result = {1'b0, signal_id};
            end else begin
                result = {7'b0, debug_data[signal_id]};
            end
        end else if (index == 7'd127) begin
            // Checksum
            result = checksum;
        end

        return result;
    endfunction

    assign tx_byte = packet_generation(byte_index);
    assign next_tx_byte = packet_generation(byte_index + 1'b1);
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Data ready stays high until a complete SPI transaction
    assign data_ready = (current_state == ready) || (current_state == transfer);
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // State register and datapath registers
    always_ff @(posedge clk_50 or negedge rst_n) begin
        if (!rst_n) begin
            current_state <= idle;

            debug_data <= '0;
            cycle_count <= '0;
            settle_count <= '0;

            byte_index <= '0;
            bit_index <= '0;
            frame_complete_flag <= 1'b0;

            spi_miso <= 1'b0;
        end else begin
            current_state <= next_state;

            case (current_state)
                idle: begin
                    if (lab_clock_rising_flag) begin
                        settle_count <= '0;
                    end
                end

                wait_settle: begin
                    if (settle_count == 3'd4) begin
                        debug_data <= probes;
                        cycle_count <= cycle_count + 1'b1;
                    end else begin
                        settle_count <= settle_count + 1'b1;
                    end
                end

                ready: begin
                    if (spi_cs_n_falling_flag) begin
                        byte_index <= '0;
                        bit_index <= 3'd7;
                        frame_complete_flag <= 1'b0;

                        // First bit of 0xA5
                        spi_miso <= 1'b1;
                    end
                end

                transfer: begin
                    // SPI mode 0: master samples on rising edge
                    if (spi_clk_rising_flag && !spi_cs_n_sync[1]) begin
                        if (byte_index == 7'd127 && bit_index == 3'd0) begin
                            frame_complete_flag <= 1'b1;
                        end
                    end

                    // FPGA updates MISO after falling edge
                    if (spi_clk_falling_flag && !spi_cs_n_sync[1] &&
                        !frame_complete_flag) begin

                        if (bit_index == 3'd0) begin
                            if (byte_index < 7'd127) begin
                                byte_index <= byte_index + 1'b1;
                                bit_index <= 3'd7;
                                spi_miso <= next_tx_byte[7];
                            end
                        end else begin
                            bit_index <= bit_index - 1'b1;
                            spi_miso <= tx_byte[bit_index - 1'b1];
                        end
                    end

                    if (spi_cs_n_rising_flag) begin
                        spi_miso <= 1'b0;
                    end
                end

                default: begin
                end
            endcase
        end
    end

    // Next-state logic
    always_comb begin
        next_state = current_state;

        case (current_state)
            idle: begin
                if (lab_clock_rising_flag) begin
                    next_state = wait_settle;
                end
            end

            wait_settle: begin
                if (settle_count == 3'd4) begin
                    next_state = ready;
                end
            end

            ready: begin
                if (spi_cs_n_falling_flag) begin
                    next_state = transfer;
                end
            end

            transfer: begin
                if (spi_cs_n_rising_flag) begin
                    if (frame_complete_flag) begin
                        next_state = idle;
                    end else begin
                        next_state = ready;
                    end
                end
            end

            default: begin
                next_state = idle;
            end
        endcase
    end
    ////////////////////////////////////////////////////////////////////////////////////////////////////

endmodule

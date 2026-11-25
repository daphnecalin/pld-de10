module spi_slave (
    input  logic        clk_50,
    input  logic        rst_n,
    input  logic        lab_clk,
    input  logic [43:0] probes,
    input  logic        spi_clk,
    input  logic        spi_cs_n,
    input  logic        spi_mosi,
    output logic        spi_miso,
    output logic        data_ready,
    output logic        command_done,
    output logic [2:0]  command_status,
    output logic [43:0] control_values,
    output logic [13:0] control_enable,
    output logic [7:0]  control_update_gray
);
    localparam int FRAME_BYTES = 128;
    localparam int NUM_SIGNALS = 14;
    localparam int SETTLE_CYCLES = 4;
    localparam int WRITE_BYTES = 11;
    localparam logic [7:0] WRITE_MARKER = 8'hC3;
    localparam logic [7:0] PROTOCOL_VERSION = 8'h01;
    localparam logic [7:0] OP_SET = 8'h01;
    localparam logic [7:0] OP_CLEAR = 8'h02;

    localparam logic [2:0] STATUS_OK = 3'd1;
    localparam logic [2:0] STATUS_BAD_FRAME = 3'd2;
    localparam logic [2:0] STATUS_BAD_ID = 3'd3;
    localparam logic [2:0] STATUS_BAD_WIDTH = 3'd4;
    localparam logic [2:0] STATUS_BAD_VALUE = 3'd5;
    localparam logic [2:0] STATUS_BAD_OPCODE = 3'd6;

    typedef enum logic [2:0] {IDLE, SETTLE, READY, TRANSFER} state_t;
    state_t state;

    logic [2:0] lab_sync, sck_sync, cs_sync, mosi_sync;
    wire lab_rise =  lab_sync[1] & ~lab_sync[2];
    wire sck_rise =  sck_sync[1] & ~sck_sync[2];
    wire sck_fall = ~sck_sync[1] &  sck_sync[2];
    wire cs_rise  =  cs_sync[1]  & ~cs_sync[2];
    wire cs_fall  = ~cs_sync[1]  &  cs_sync[2];

    logic [2:0] settle_count;
    logic [15:0] sample_count;
    logic [43:0] sample_probes;
    logic [7:0] control_update_count;
    logic sample_pending;
    logic transfer_has_sample;
    logic [7:0] frame [0:FRAME_BYTES-1];
    logic [6:0] byte_index;
    logic [2:0] bit_index;
    logic [10:0] bits_sampled;

    logic [7:0] rx_shift;
    logic [2:0] rx_bit_index;
    logic [7:0] rx_byte_count;
    logic [7:0] command_bytes [0:WRITE_BYTES-1];

    assign data_ready = sample_pending;

    function automatic logic [7:0] signal_width(input int id);
        case (id)
            0, 1, 2, 3: return 8'd1;
            5, 6: return 8'd2;
            7: return 8'd3;
            default: return 8'd4;
        endcase
    endfunction

    function automatic logic [31:0] signal_value(input int id, input logic [43:0] snapshot);
        case (id)
            0: return {31'b0, snapshot[0]};
            1: return {31'b0, snapshot[1]};
            2: return {31'b0, snapshot[2]};
            3: return {31'b0, snapshot[3]};
            4: return {28'b0, snapshot[7:4]};
            5: return {28'b0, snapshot[11:8]};
            6: return {28'b0, snapshot[15:12]};
            7: return {28'b0, snapshot[19:16]};
            8: return {28'b0, snapshot[23:20]};
            9: return {28'b0, snapshot[27:24]};
            10: return {28'b0, snapshot[31:28]};
            11: return {28'b0, snapshot[35:32]};
            12: return {28'b0, snapshot[39:36]};
            13: return {28'b0, snapshot[43:40]};
            default: return 32'b0;
        endcase
    endfunction

    function automatic logic writable_id(input logic [7:0] id);
        case (id)
            1, 2, 4, 5, 6, 7: return 1'b1;
            default: return 1'b0;
        endcase
    endfunction

    function automatic logic [31:0] command_value(input logic [7:0] bytes [0:WRITE_BYTES-1]);
        return {bytes[9], bytes[8], bytes[7], bytes[6]};
    endfunction

    always_ff @(posedge clk_50 or negedge rst_n) begin : controller
        int offset;
        int nbytes;
        int width;
        logic [31:0] value;
        logic [7:0] sum;
        logic [7:0] check_sum;
        logic command_valid;
        logic [7:0] command_id;
        logic [7:0] command_width;
        logic [7:0] command_opcode;
        logic [7:0] next_control_generation;
        if (!rst_n) begin
            lab_sync <= '0;
            sck_sync <= '0;
            cs_sync <= '1;
            mosi_sync <= '0;
            state <= IDLE;
            settle_count <= '0;
            sample_count <= '0;
            sample_probes <= '0;
            sample_pending <= 1'b0;
            transfer_has_sample <= 1'b0;
            byte_index <= '0;
            bit_index <= 3'd7;
            bits_sampled <= '0;
            rx_shift <= '0;
            rx_bit_index <= 3'd7;
            rx_byte_count <= '0;
            spi_miso <= 1'b0;
            command_done <= 1'b0;
            command_status <= 3'd0;
            control_values <= '0;
            control_enable <= '0;
            control_update_count <= 8'd0;
            control_update_gray <= 8'd0;
            for (int k = 0; k < FRAME_BYTES; k++) frame[k] <= 8'h00;
            for (int k = 0; k < WRITE_BYTES; k++) command_bytes[k] <= 8'h00;
        end else begin
            lab_sync <= {lab_sync[1:0], lab_clk};
            sck_sync <= {sck_sync[1:0], spi_clk};
            cs_sync <= {cs_sync[1:0], spi_cs_n};
            mosi_sync <= {mosi_sync[1:0], spi_mosi};

            case (state)
                IDLE: begin
                    if (lab_rise && !sample_pending) begin
                        settle_count <= '0;
                        state <= SETTLE;
                    end else if (cs_fall) begin
                        transfer_has_sample <= 1'b0;
                        byte_index <= 0;
                        bit_index <= 7;
                        bits_sampled <= 0;
                        rx_shift <= 0;
                        rx_bit_index <= 7;
                        rx_byte_count <= 0;
                        command_done <= 1'b0;
                        spi_miso <= 1'b0;
                        state <= TRANSFER;
                    end
                end
                SETTLE: begin
                    if (settle_count == SETTLE_CYCLES - 1) begin
                        sample_probes <= probes;
                        sample_count <= sample_count + 16'd1;
                        sample_pending <= 1'b1;
                        sum = 8'h00;
                        for (int k = 0; k < FRAME_BYTES; k++) frame[k] <= 8'h00;
                        frame[0] <= 8'hA5;
                        frame[1] <= 8'h01;
                        frame[2] <= (sample_count + 16'd1) & 16'h00FF;
                        frame[3] <= (sample_count + 16'd1) >> 8;
                        sum = 8'hA5 ^ 8'h01 ^ 8'((sample_count + 16'd1)) ^ 8'((sample_count + 16'd1) >> 8);
                        offset = 4;
                        for (int id = 0; id < NUM_SIGNALS; id++) begin
                            width = int'(signal_width(id));
                            nbytes = (width + 7) / 8;
                            value = signal_value(id, probes);
                            frame[offset] <= 8'(id);
                            frame[offset+1] <= 8'(width);
                            sum = sum ^ 8'(id) ^ 8'(width);
                            for (int b = 0; b < nbytes; b++) begin
                                frame[offset+2+b] <= 8'(value >> (b*8));
                                sum = sum ^ 8'(value >> (b*8));
                            end
                            offset = offset + 2 + nbytes;
                        end
                        frame[127] <= sum;
                        state <= READY;
                    end else settle_count <= settle_count + 1'b1;
                end
                READY: begin
                    if (cs_fall) begin
                        transfer_has_sample <= 1'b1;
                        byte_index <= 0;
                        bit_index <= 7;
                        bits_sampled <= 0;
                        rx_shift <= 0;
                        rx_bit_index <= 7;
                        rx_byte_count <= 0;
                        command_done <= 1'b0;
                        spi_miso <= frame[0][7];
                        state <= TRANSFER;
                    end
                end
                TRANSFER: begin
                    if (cs_rise) begin
                        spi_miso <= 1'b0;
                        command_valid = (rx_byte_count == WRITE_BYTES) &&
                                        (command_bytes[0] == WRITE_MARKER);
                        if ((rx_byte_count != 0) && (command_bytes[0] == WRITE_MARKER)) begin
                            command_done <= 1'b1;
                            if (!command_valid) begin
                                command_status <= STATUS_BAD_FRAME;
                            end else begin
                                check_sum = 0;
                                for (int k = 0; k < WRITE_BYTES-1; k++)
                                    check_sum = check_sum ^ command_bytes[k];
                                command_id = command_bytes[4];
                                command_width = command_bytes[5];
                                command_opcode = command_bytes[2];
                                value = command_value(command_bytes);
                                if (command_bytes[1] != PROTOCOL_VERSION || check_sum != command_bytes[10]) begin
                                    command_status <= STATUS_BAD_FRAME;
                                end else if (!writable_id(command_id)) begin
                                    command_status <= STATUS_BAD_ID;
                                end else if (command_width != signal_width(command_id)) begin
                                    command_status <= STATUS_BAD_WIDTH;
                                end else if (value >> command_width) begin
                                    command_status <= STATUS_BAD_VALUE;
                                end else if (command_opcode != OP_SET && command_opcode != OP_CLEAR) begin
                                    command_status <= STATUS_BAD_OPCODE;
                                end else begin
                                    if (command_opcode == OP_SET) begin
                                        case (command_id)
                                            1: control_values[1] <= value[0];
                                            2: control_values[2] <= value[0];
                                            4: control_values[7:4] <= value[3:0];
                                            5: control_values[9:8] <= value[1:0];
                                            6: control_values[13:12] <= value[1:0];
                                            7: control_values[18:16] <= value[2:0];
                                            default: ;
                                        endcase
                                        control_enable[command_id] <= 1'b1;
                                    end else begin
                                        control_enable[command_id] <= 1'b0;
                                    end
                                    next_control_generation = control_update_count + 8'd1;
                                    control_update_count <= next_control_generation;
                                    control_update_gray <= (next_control_generation >> 1) ^ next_control_generation;
                                    command_status <= STATUS_OK;
                                end
                            end
                        end

                        if (transfer_has_sample && bits_sampled == FRAME_BYTES * 8 && !command_valid) begin
                            sample_pending <= 1'b0;
                            state <= IDLE;
                        end else begin
                            state <= transfer_has_sample ? READY : IDLE;
                        end
                    end else if (!cs_sync[1]) begin
                        if (sck_rise) begin
                            if (bits_sampled < FRAME_BYTES * 8)
                                bits_sampled <= bits_sampled + 1'b1;
                            rx_shift <= {rx_shift[6:0], mosi_sync[1]};
                            if (rx_bit_index == 0) begin
                                if (rx_byte_count < WRITE_BYTES)
                                    command_bytes[rx_byte_count] <= {rx_shift[6:0], mosi_sync[1]};
                                if (rx_byte_count != 8'hFF)
                                    rx_byte_count <= rx_byte_count + 1'b1;
                                rx_bit_index <= 7;
                            end else begin
                                rx_bit_index <= rx_bit_index - 1'b1;
                            end
                        end
                        if (transfer_has_sample && sck_fall && bits_sampled < FRAME_BYTES * 8) begin
                            if (bit_index == 0) begin
                                if (byte_index < FRAME_BYTES - 1) begin
                                    byte_index <= byte_index + 1'b1;
                                    bit_index <= 7;
                                    spi_miso <= frame[byte_index+1'b1][7];
                                end
                            end else begin
                                bit_index <= bit_index - 1'b1;
                                spi_miso <= frame[byte_index][bit_index-1'b1];
                            end
                        end
                    end
                end
                default: state <= IDLE;
            endcase
        end
    end
endmodule

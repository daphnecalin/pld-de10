module spi_slave (
    input  logic        clk_50,
    input  logic        rst_n,
    input  logic        lab_clk,
    input  logic [43:0] probes,
    input  logic        spi_clk,
    input  logic        spi_cs_n,
    input  logic        spi_mosi, // reserved; read-only SPI protocol
    output logic        spi_miso,
    output logic        data_ready
);
    // Fixed 128-byte frame: A5, version 01, sample counter LE,
    // 14 records (ID, bit-width, packed value), zero padding, XOR at byte 127.
    localparam int FRAME_BYTES = 128;
    localparam int NUM_SIGNALS = 14;
    localparam int RECORD_END  = 46; // first byte after 42 record bytes
    localparam int SETTLE_CYCLES = 4;

    typedef enum logic [1:0] {IDLE, SETTLE, READY, TRANSFER} state_t;
    state_t state;

    logic [2:0] lab_sync, sck_sync, cs_sync;
    wire lab_rise =  lab_sync[1] & ~lab_sync[2];
    wire sck_rise =  sck_sync[1] & ~sck_sync[2];
    wire sck_fall = ~sck_sync[1] &  sck_sync[2];
    wire cs_rise  =  cs_sync[1]  & ~cs_sync[2];
    wire cs_fall  = ~cs_sync[1]  &  cs_sync[2];

    logic [2:0] settle_count;
    logic [15:0] sample_count;
    logic [43:0] sample_probes;
    logic [7:0] frame [0:FRAME_BYTES-1];
    logic [6:0] byte_index;
    logic [2:0] bit_index;
    logic [10:0] bits_sampled; // 0..1024

    // The frame is constructed once per snapshot and cannot change mid-transfer.
    // Constant mapping matches debug_ip.sv. Future generator should emit both.
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

    assign data_ready = (state == READY) || (state == TRANSFER);

    always_ff @(posedge clk_50 or negedge rst_n) begin : controller
        int offset;
        int nbytes;
        int width;
        logic [31:0] value;
        logic [7:0] sum;
        if (!rst_n) begin
            lab_sync <= '0;
            sck_sync <= '0;
            cs_sync <= '1;
            state <= IDLE;
            settle_count <= '0;
            sample_count <= '0;
            sample_probes <= '0;
            byte_index <= '0;
            bit_index <= 3'd7;
            bits_sampled <= '0;
            spi_miso <= 1'b0;
            for (int k = 0; k < FRAME_BYTES; k++) frame[k] <= 8'h00;
        end else begin
            lab_sync <= {lab_sync[1:0], lab_clk};
            sck_sync <= {sck_sync[1:0], spi_clk};
            cs_sync <= {cs_sync[1:0], spi_cs_n};

            case (state)
                IDLE: begin
                    if (lab_rise) begin
                        settle_count <= '0;
                        state <= SETTLE;
                    end
                end
                SETTLE: begin
                    if (settle_count == SETTLE_CYCLES - 1) begin
                        sample_probes <= probes;
                        sample_count <= sample_count + 16'd1;
                        // Generate frame from CURRENT probes, not stale snapshot.
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
                    // Refuse to start until CS is observed low.
                    if (cs_fall) begin
                        byte_index <= 0;
                        bit_index <= 7;
                        bits_sampled <= 0;
                        spi_miso <= frame[0][7]; // pre-load MSB before first rising SCK
                        state <= TRANSFER;
                    end
                end
                TRANSFER: begin
                    if (cs_rise) begin
                        spi_miso <= 1'b0;
                        // Only ACK a complete frame. Retry reads the same snapshot.
                        if (bits_sampled == FRAME_BYTES * 8) state <= IDLE;
                        else state <= READY;
                    end else if (!cs_sync[1]) begin
                        if (sck_rise && bits_sampled < FRAME_BYTES * 8)
                            bits_sampled <= bits_sampled + 1'b1;
                        if (sck_fall && bits_sampled < FRAME_BYTES * 8) begin
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

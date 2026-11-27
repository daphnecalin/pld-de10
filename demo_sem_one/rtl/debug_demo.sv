module debug_demo (
    input logic clk_50,
    input logic rst_n,

    input logic lab_clk,
    input logic s_l,
    input logic cin,

    input logic [3:0] input_data,
    input logic [1:0] msa,
    input logic [1:0] msb,
    input logic [2:0] msc,

    input logic spi_clk,
    input logic spi_cs_n,
    input logic spi_mosi,

    output logic spi_miso,
    output logic data_ready,
    output logic command_done,
    output logic [2:0] command_status,

    output wire cout,
    output wire [3:0] reg_a,
    output wire [3:0] reg_b,
    output wire [3:0] output_data,
    output wire [3:0] a_and_b,
    output wire [3:0] a_or_b,
    output wire [3:0] a_plus_b
);

    logic [43:0] probes;
    logic [43:0] control_values;
    logic [13:0] control_enable;
    logic [7:0] control_update_gray;
    logic [7:0] control_update_meta, control_update_sync, control_update_seen;
    logic [43:0] control_values_applied;
    logic [13:0] control_enable_applied;
    logic s_l_effective, cin_effective;
    logic [3:0] input_effective;
    logic [1:0] msa_effective, msb_effective;
    logic [2:0] msc_effective;

    // Firmware overrides selected lab inputs. With an override disabled,
    // the DE-10 Lite's physical input remains in control.
    assign s_l_effective = control_enable_applied[1] ? control_values_applied[1] : s_l;
    assign cin_effective = control_enable_applied[2] ? control_values_applied[2] : cin;
    assign input_effective = control_enable_applied[4] ? control_values_applied[7:4] : input_data;
    assign msa_effective = control_enable_applied[5] ? control_values_applied[9:8] : msa;
    assign msb_effective = control_enable_applied[6] ? control_values_applied[13:12] : msb;
    assign msc_effective = control_enable_applied[7] ? control_values_applied[18:16] : msc;

    // The SPI controller runs from clk_50. Transfer the stable shadow values
    // into the lab clock domain when a validated write toggles the request.
    always_ff @(posedge lab_clk or negedge rst_n) begin
        if (!rst_n) begin
            control_update_meta <= 8'd0;
            control_update_sync <= 8'd0;
            control_update_seen <= 8'd0;
            control_values_applied <= '0;
            control_enable_applied <= '0;
        end else begin
            control_update_meta <= control_update_gray;
            control_update_sync <= control_update_meta;
            if (control_update_sync != control_update_seen) begin
                control_values_applied <= control_values;
                control_enable_applied <= control_enable;
                control_update_seen <= control_update_sync;
            end
        end
    end

    lab4_ralu_sample ralu_inst (
        .CLK(lab_clk),
        .S_L(s_l_effective),
        .CIN(cin_effective),

        .Input(input_effective),
        .MSA(msa_effective),
        .MSB(msb_effective),
        .MSC(msc_effective),

        .COUT(cout),
        .RegA(reg_a),
        .RegB(reg_b),
        .Output(output_data),
        .A_and_B(a_and_b),
        .A_or_B(a_or_b),
        .A_plus_B(a_plus_b)
    );

    debug_ip debug_inst (
        .clk(lab_clk),
        .s_l(s_l_effective),
        .cin(cin_effective),
        .cout(cout),

        .input_data(input_effective),
        .msa({2'b00, msa_effective}),
        .msb({2'b00, msb_effective}),
        .msc({1'b0, msc_effective}),
        .reg_a(reg_a),
        .reg_b(reg_b),

        .output_data(output_data),
        .a_and_b(a_and_b),
        .a_or_b(a_or_b),
        .a_plus_b(a_plus_b),

        .probes(probes)
    );

    spi_slave spi_inst (
        .clk_50(clk_50),
        .rst_n(rst_n),
        .lab_clk(lab_clk),

        .probes(probes),

        .spi_clk(spi_clk),
        .spi_cs_n(spi_cs_n),
        .spi_mosi(spi_mosi),

        .spi_miso(spi_miso),
        .data_ready(data_ready),
        .command_done(command_done),
        .command_status(command_status),
        .control_values(control_values),
        .control_enable(control_enable),
        .control_update_gray(control_update_gray)
    );

endmodule

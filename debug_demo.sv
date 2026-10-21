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

    output wire cout,
    output wire [3:0] reg_a,
    output wire [3:0] reg_b,
    output wire [3:0] output_data,
    output wire [3:0] a_and_b,
    output wire [3:0] a_or_b,
    output wire [3:0] a_plus_b
);

    logic [43:0] probes;

    lab4_ralu_sample ralu_inst (
        .CLK(lab_clk),
        .S_L(s_l),
        .CIN(cin),

        .Input(input_data),
        .MSA(msa),
        .MSB(msb),
        .MSC(msc),

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
        .s_l(s_l),
        .cin(cin),
        .cout(cout),

        .input_data(input_data),
        .msa({2'b00, msa}),
        .msb({2'b00, msb}),
        .msc({1'b0, msc}),
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
        .data_ready(data_ready)
    );

endmodule

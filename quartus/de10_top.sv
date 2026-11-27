module de10_top (
    input logic clk_50,
    input logic rst_n,

    input logic lab_clk,
    // input logic s_l, // (tied to true)
    // input logic cin, // (tied to false)

    input logic [1:0] input_data, // upper 2 bits are hardcoded to 0
    input logic [1:0] msa,
    input logic [1:0] msb,
    input logic [2:0] msc,

    input logic spi_clk,
    input logic spi_cs_n,
    input logic spi_mosi,

    output logic spi_miso,
    output logic data_ready,
    output logic command_done,
    output logic [2:0] command_status

    //output wire cout,
    //output wire [3:0] reg_a,
    //output wire [3:0] reg_b,
    //output wire [3:0] output_data,
    //output wire [3:0] a_and_b,
    //output wire [3:0] a_or_b,
    //output wire [3:0] a_plus_b
);


    debug_demo debug_demo_instance (
        .clk_50(clk_50),
        .rst_n(rst_n),
        .lab_clk(lab_clk),
        .s_l(1'b0),
        .cin(1'b0),
        .input_data({2'b00, input_data}),
        .msa(msa),
        .msb(msb),
        .msc(msc),
        .spi_clk(spi_clk),
        .spi_cs_n(spi_cs_n),
        .spi_mosi(spi_mosi),
        .spi_miso(spi_miso),
        .data_ready(data_ready),
        .command_done(command_done),
        .command_status(command_status),
        .cout(),
        .reg_a(),
        .reg_b(),
        .output_data(),
        .a_and_b(),
        .a_or_b(),
        .a_plus_b()
    );

endmodule
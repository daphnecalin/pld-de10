module debug_ip (
    input logic clk,
    input logic s_l,
    input logic cin,
    input logic cout,

    input logic [3:0] input_data,
    input logic [3:0] msa,
    input logic [3:0] msb,
    input logic [3:0] msc,
    input logic [3:0] reg_a,
    input logic [3:0] reg_b,

    input logic [3:0] output_data,
    input logic [3:0] a_and_b,
    input logic [3:0] a_or_b,
    input logic [3:0] a_plus_b,

    output logic [43:0] probes
);
    always_comb begin
        probes = '0;

        probes[0] = clk;
        probes[1] = s_l;
        probes[2] = cin;
        probes[3] = cout;

        probes[7:4] = input_data;
        probes[11:8] = msa;
        probes[15:12] = msb;
        probes[19:16] = msc;

        probes[23:20] = reg_a;
        probes[27:24] = reg_b;

        probes[31:28] = output_data;
        probes[35:32] = a_and_b;
        probes[39:36] = a_or_b;
        probes[43:40] = a_plus_b;
    end
endmodule

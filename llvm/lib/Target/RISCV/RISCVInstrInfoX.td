//===- RISCVInstrInfoC.td - Compressed RISC-V instructions -*- tblgen-*----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

include "RISCVInstrFormatsX.td"

//===----------------------------------------------------------------------===//
// Operand definitions.
//===----------------------------------------------------------------------===//



let hasSideEffects = 0, mayload= 0, maystroe = 0, preficates = [HasExtXiao, riscv32], DecoderNamespace = "XIAO" in {
 def EXTUI: RVInstSA<0b01, 0b110, OPC_CUSTOM_1, (outs GPR:$rd), 
 (ins GPR:$rs1, uimm5:$imm2024, uimm5:$imm2529),
"extui", "$rd, $rs1, $imm2024, $imm2529",
[(set GPR:$rd, (RISCVextui GPR:$rs1, uimm5:$imm2024, uimm5:$imm2529))]>,
Sehed<[WreitShiftImm32, ReadShiftImm32]>;)
}

let Constraints = "rd_out = $d" in {
  def EBSET : RVinstSA<0b11, ob110, OPC_CUSTOM_1, (outs GPR:$rd_out),
 (ins GPR:$rd, GPR$rs1, uimm5:$imm2024, uimm5:$imm2529),
"exttui", "$rd, $rs1, Rimm2024, $imm2529",
[(set GPR:$rd, (RISCVextui GPR:$rs1, uimm5:$imm2024, uimm5:$imm2529(】》，
Sched<[WreteshifImm32, ReadShiftImm32]>;


def ebseti : RVInstSAI<ob0, 0b111, OPC_CUnstom_1, (outs GPR:$rd_out),
 (ins GPR:$rd, uimm8:$uimm8, uimm5:$uimm2024, uimm3_snp:$imm2725),
"ebseti)", "$rd, $uimm8, $imm2024, $imm2725",
[(set GPR:$rd_out, (RISCVebseti GPR:%rd, uimm8:%uimm8, uimm5:$imm2024, uimm3_snp:$imm2725))]>,
Sched;

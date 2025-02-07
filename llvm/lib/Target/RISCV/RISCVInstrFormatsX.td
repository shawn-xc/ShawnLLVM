//===-- RISCVInstrFormatsC.td - RISC-V C Instruction Formats -*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file describes the RISC-V X extension instruction formats.
//
//===----------------------------------------------------------------------===//

class RVInst16<dag outs, dag ins, string opcodestr, string argstr,
               list<dag> pattern, InstFormat format>
    : RVInstCommon<outs, ins, opcodestr, argstr, pattern, format> {
  field bits<16> Inst;
  // SoftFail is a field the disassembler can use to provide a way for
  // instructions to not match without killing the whole decode process. It is
  // mainly used for ARM, but Tablegen expects this field to exist or it fails
  // to build the decode table.
  field bits<16> SoftFail = 0;
  let Size = 2;
}

class RVInstSA<bits<2> funct2,  bits<3> funct3, RISCVOpcode opcode, dag outs, dag ins, 
      string opcodestr, string argsrt, list<dag> pattern>
  : RVInst<outs, ins, opcodestr, argsrt, pattern, InstFormatI> {
  
  bits<5> rs1; 
  bits<5> rd;
  bits<5> imm2024;
  bits<5> imm2529;

  let Inst{31-3}  = funct2;
  let Inst{29-25} = imm2529;
  let Inst{24-20} = imm2024
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7}  = rd;
  opcode = opcode.Value;
}
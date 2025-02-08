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

def OPC_ACE     : RISCVOpcode<"ACE",  0b1111011>;


class RVInstACE<bits<7> funct7, bits<4> funct4, RISCVOpcode opcode, dag outs, dag ins,
      string opcodestr, string argstr>
  : RVInst<outs, ins, opcodestr, argstr, [], InstFormatACE> {
  bits<5> rs1;
  bits<5> rs2;
  bits<3> ace3;
  bits<1> imm1;

  let Inst{31-25} = funct7;
  let Inst{24-20} = rs1;
  let Inst{19-15} = rs2;
  let Inst{14-11} = funct4;
  let Inst{10} = imm1;
  let Inst{9-7} = ace3;
  let Inst{6-0} = opcode.Value;
}

class RVInstSA<bits<2> funct2,  bits<3> funct3, RISCVOpcode opcode, dag outs, dag ins, 
      string opcodestr, string argsrt, list<dag> pattern>
  : RVInst<outs, ins, opcodestr, argsrt, pattern, InstFormatI> {
  
  bits<5> rs1; 
  bits<5> rd;
  bits<5> imm2024;
  bits<5> imm2529;

  let Inst{31-30}  = funct2;
  let Inst{29-25} = imm2529;
  let Inst{24-20} = imm2024;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7}  = rd;
  let Inst{6-0} = opcode.Value;
}
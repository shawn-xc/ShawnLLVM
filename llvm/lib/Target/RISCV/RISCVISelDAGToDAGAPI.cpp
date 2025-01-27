//===-- RISCVISelDAGToDAG.cpp - A dag to dag inst selector for RISC-V -----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the RISC-V target.
//
//===----------------------------------------------------------------------===//

#include "RISCVISelDAGToDAG.h"
#include "MCTargetDesc/RISCVBaseInfo.h"
#include "MCTargetDesc/RISCVMCTargetDesc.h"
#include "MCTargetDesc/RISCVMatInt.h"
#include "RISCVISelLowering.h"
#include "RISCVMachineFunctionInfo.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// 获取当前node的数据size
unsigned RISCVDAGToDAGISel::getCurNodeSize(SDValue &Node) {
  unsigned size = 32;
  switch (Node.getOpcode()) {
    default : break;
    case ISD::LOAD: {  // LOAD的数据大小
      LoadSDNode *LD  = dyn_cast<LoadSDNode>(Node);
      MachineMemOperand *MMO = LD->getMemOperand();
      if (LD->getExtensionType() != ISD::SEXTLOAD) // 有符号数需要考虑符号位的影响
        size = MMO->getSize().getValue() * 8;
      break;
    }

    case ISD::AssertZext: { //清零的AND 可能是assertzext
      VTSDNode * VTNode = cast<VTSDNode>(Node.getOperand(1));
      EVT zextVt = VTNode->getVT();
      if (zextVt.isSimple())
        size = zextVt.getSimpleVT().getSizeInBits();
      break;
    }

    case ISD::SETCC:
      size = 1;
      break;
  }

  return size;
}

// 判定当前立即数是否符合获取Get语义
bool RISCVDAGToDAGISel::isGetField(unsigned totBits, uint32_t v, 
                                  unsigned &startpos, unsigned &numbits) {
                                    
/* 获取语义经典值类型：
   1. 32bit数据容器内容：{000(小size数据零扩展) + 000111000（真实的totBits长度）}
   2. 32bit数据容器内容：{0000(零扩展) + 111000(真实totBits)
*/

  // totBits不是32bits的话，v需要mask掉符号位
  if (totBits != 32) {
    uint32_t maskTrail = maskTrailingOnes<uint32_t>(totBits);
    v = v & maskTrail;
  }

  // 获取最左侧的000
  startpos = countr_zero(v);
  if (startpos == totBits)
    return false;

  // 获取中间的111
  numbits = countr_one(v >> startpos); 
  if (numbits == 0)
    return false;

  // corner : 已经凑够32bit不需要再右移了，因为一个数右移32bit等于自身
  if ((startpos + numbits) == totBits)
    return true;

  // 获取最右侧的000 
  unsigned leftNum = countr_zero(v >> (startpos + numbits));
  if (leftNum != 32)
    return false;
  
  return true;
}

bool RISCVDAGToDAGISel::isClearField(unsigned totBits, uint32_t v, 
                              unsigned &startpos, unsigned &numbits) {
/* 清零语义经典值
   1. 32bit容器内容：{000 (小size扩展u32前置补0） + 11110011 (真实totbits)}
   2. 32bit容器内容  0001111..11(共32bit)
*/

  // 仅保留totBits范围内的内容
  if (totBits != 32) { 
   uint32_t maskTrail = maskTrailingOnes<uint32_t>(totBits);
   v = v & maskTrail;
  }

  // 获取最左侧的111
  startpos = countr_one(v);
  if (startpos == totBits)
    return false;

  // 获取中间000 也可能一直获取到最高位
  numbits = countr_zero(v >> startpos);
  if (numbits == 0)
    return false;

  // 获取最高的32bit有2个可能
  // 1) 原生byte,half的数据通过零扩展扩展填充  2)本身就是word数据
  if (numbits == 32) {
    numbits = totBits - startpos;

    if(totBits != 32)
      numbits = (32 - totBits) + numbits;
    return true;
  }

  // 如果此时numbits不是32， 则一位置还有1， 这个1一定实在0的左边
  // 获取最左侧的111
  unsigned leftNum = countr_one(v >> (startpos + numbits));
  if (startpos + numbits + leftNum != totBits)
   return false;

  return true;
}

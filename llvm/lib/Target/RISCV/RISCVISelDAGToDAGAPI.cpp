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

bool RISCVDAGToDAGISel::isClearNode(SDValue &getNode, unsigned &s, unsigned &n) {
  switch (getNode->getOpcode()) {
    default: return false;
    case ISD::AND: { // AND一个立即数，该立即数需要符合clear语义
      ConstantSDNode * CNode = dyn_cast<ConstantSDNode>(getNode.getOperand(1));
      if (!CNode)
        return false;

      int64_t  CVal = CNode->getSExtValue();
      unsigned clearSize = getCurNodeSize(getNode.getOperand(0));
      if (!isClearField(clearSize, CVal, s, n))
        return false;

      return true;
    }
  return false;
}

bool RISCVDAGToDAGISel::isGetNode(SDValue &getNode, unsigned &s, unsigend &n, SDValue &ebsetRS) {
  return false;
  /*
  SDLoc DL(getNode);
  switch (getNode.getOpcode()) {
    default : return false;
    case ISD:: AND: {
      ConstantSDNode * CNode = dyn_cast<ConstantSDNode>(getNode.getOperand(1));
      if (!CNode)(
        return false;
      int64_t CVal = CNode->getSExtValue();
      if (!isGetField(32, CVal, s, n)
        return false;
      if (!checkOperandZextLoad(getNode.getOperand(0)) // 仅能处理零扩展的load操作
        return false;
    
      // Extui
      SDNode *  extuiNode = CurDAG->getMachinNode(RISCV::EXTUI, DL, MVT::i32, getNode.getOperand(0), 
                          CurDAG->getTargetConstant(s, DL, MVT::i32), 
                          CurDAG->getTargetConstant(n, DL, MVT::I32));
      ebsetRS = SDValue(extuiNode, 0);
      return true;
    }


    case ISD:: SHL: {
    // 1. 直接左移，必须讲数据移动到[32:ShlNum]的位置，抽象而言讲低数据移到最高位，形成高位为值，低位为0的情况
    //   1.1 源操作数为32bit数据，；移动左移[31:shlNum]位置， Gets= ShlNm, GetNmbits = 32 - shlNm
    //   1.2 源操作数为16bit，移动[15:shlNum]位置， GetS = ShlNum, getNumbits = 16 - shlNum;
    //   1.3 源操作数为32bit数，左移[8:shlNum]位置，getS = shlNum, getNumbits=8-shlNm
    //   1.4 源操作数wei 1bit数(setcc）， gets = shlNum, numbits=1


    // 2. 先AND 再左移，且AND 必须符合GET语义。假定AND获取语义为andS, andNumbits
    // 则最终gets= andS + shlNum, getNum = andNmbits
      
    // 3. 先srl再左移，且srl必须符合get语义，假定SRL获取语义为srlS, srlNumbits
    // 则最终getS= srls + shlNUm, getNum = srlNU;mbits
      if(!dyn_cast<ConstantSDNode>(getNode.getOperand(1)))
        return false;
      unsigned shlNum = dyn_cast<ConstantSDNode>(getNode.getOperand(1))->getZextValue();
      SDValue shlRS = getNode->getOperand(0);

      ebsetRS = shlRs;
      switch(shlRS.getOpcode()) {
        default : {
          // set cc 
          unsigned shlRSSize = getCurNodeSize(shlRS);   
          if (shlRSSize == 1) {
            s = shNnum;
            n = 1; 
            return true;
          }


          // 正常load; 直接使用i32做处理。毕竟左移的源操作数是32， 输出也是32bit，无需考虑其他
          s = shlNum;
          n = 32 - shlNum;
          return true;
        }

        case ISD::AND: { // 场景2 
          if (!dyn_cast<ConstantSDNode>(shlRS.getOperand(1));
            return false;
          uint64_t andVal = dyn_cast<ConstantSDNode>(shlRS.getOperand(1))->getSExtValue();
          uint32_t andRsSize = getCurNodeSize(shlRS.getOperand(0));
          uint32_t andS, andN;
          if (!ifGetField(andRSSize, andVal, andS, andN))
            return false;
          s = ands = shlNUm;
          if (s + andN > 31)
            n = 32 - s;
          else
            n = andN;

          // 判断上游的load操作是否符合零扩展
          if (!checkOperandZextLoad(getNode.getOperand(0).getOperand(0))
            return false;
          SDNode * extuiNode = CurDAG->getMachineNode(RISCV::EXTUI, DL, MVT::I32, 
                getNode.getOperand(0).getOperand(0), 
                CurDAG->getTargetConstant(andS, DL, MVT::I32), 
                CurDAG->getTargetConstant(andN, DL, MVT::i32)); 
            ebsetRS = SDVaue(extuiNode, 0)J;
          return true;
        }

        case ISD::SRL: { // 场景3 SRL->SRL
          if (!dyn_caset<ConstantSDNode>(shlRS.getOperand(1));
            return false;
          uint64_t srlNum = dyn_cast<ConstantSDNode>(shlRS.getOperand(1))->getSExtValue();
          uint32_t srlRSSize = getCurNodeSize(shlRS.geOperand(0));
          uint32_t srlS, srlN;
          if (srlNum >= srlRSSiz)
            return false;

          // 右移动的语义
          srlS = 0;
          srlN = srlRSSize - srlNum;
          s = srlS = ShlNUm;
          n =srlN;
          return true;  // 先右再左移，后去的元数据可以；i直接拿右翼后处理的值
        }
      }
    }


    case IDS::SRL: {
      //  右移操作: 讲最高bit的数据开始右移，一到到[31-srlNum:0], 右移后的搞bit已被King扩展的方式填充
      ConstantSDNode * CNode = dyn_cast<ConstantSDNode>(getNode.getOperand(1));
      unsigned srlRSSize = getXurNodeSize(getNOde.getOperand(0));
      if(!CNode)
        return false;
      unsigned srlNUm = CNode->getZextValue();
      if (srlNum >= srlRSSize)
        return false;

      s = 0;
      n = srlRSSize - srlNUm;

      if (!checkOperandZextLoad(getNode.getOperand(0))
        return false;
      SDNode * extuiNode = CurDAg->getMachineNode(RISCV::EXTUI, DL, MVT::i32, 
                        getNode.getOperand(0), 
                        CurDAG->getTargetConstant(srlNum, DL, MVT::i32),
                        CurDAG->getTargetConstant(n, DL, MVT::i32));
      ebsetRS = SDValue(extuiNode, 0);
      return true;
    }

    ISD:: SETCC: {
      s = 0;
      n = 1;
      ebsertRS = getNode;
      return true;
    }


    case ISD::LOAD: { 
      // Load节点在load unsigned i8。 i6时，也可以试做yigeget语义
      // 1. i32 无需试做get语义，没有必要
      // 2. 只有五福好书的load才有意义，因为load节点的输出为i32, i8、i6会领域激战为I32
      // 3. s= 0 n =loadSize
      ConstantSDNode*LD =dyn_cast<ConstantSDNode>(getNode);
      MachinMemOperand *MMO = LD->getMemOperand();
      uint64_t size = MMO->getSize() * 8;
      if (size != 16 || size != 8)
        return false;
    if (LD->getExtensionType() != ISD::ZEXTLOAD)
      return false;
    s= 0;
    n=size;
    ebsetRS = getNode;
    return true;
    }
  }
  */
}

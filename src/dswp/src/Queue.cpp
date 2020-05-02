/*
 * Copyright 2016 - 2019  Angelo Matni, Simone Campanoni
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "DSWP.hpp"

using namespace llvm;

void DSWP::registerQueue (
  Parallelization &par,
  LoopDependenceInfo *LDI,
  DSWPTask *fromStage,
  DSWPTask *toStage,
  Instruction *producer,
  Instruction *consumer
) {

  /*
   * Find/create the push queue in the producer stage
   */
  int queueIndex = this->queues.size();
  QueueInfo *queueInfo = nullptr;
  for (auto queueI : fromStage->producerToQueues[producer]) {
    if (this->queues[queueI]->toStage != toStage->order) continue;
    queueIndex = queueI;
    queueInfo = this->queues[queueIndex].get();
    break;
  }
  if (queueIndex == this->queues.size()) {
    this->queues.push_back(std::move(std::make_unique<QueueInfo>(producer, consumer, producer->getType())));
    fromStage->producerToQueues[producer].insert(queueIndex);
    queueInfo = this->queues[queueIndex].get();

    /*
     * Confirm a new queue is of a size handled by the parallelizer
     */
    auto& queueTypes = par.queues.queueSizeToIndex;
    bool byteSize = queueTypes.find(queueInfo->bitLength) != queueTypes.end();
    if (!byteSize) {
      errs() << "NOT SUPPORTED BYTE SIZE (" << queueInfo->bitLength << "): "; producer->getType()->print(errs()); errs() <<  "\n";
      producer->print(errs() << "Producer: "); errs() << "\n";
      abort();
    }
  }

  /*
   * Track queue indices in stages
   */
  fromStage->pushValueQueues.insert(queueIndex);
  toStage->popValueQueues.insert(queueIndex);
  toStage->producedPopQueue[producer] = queueIndex;

  /*
   * Track the stages this queue communicates between
   */
  queueInfo->consumers.insert(consumer);
  queueInfo->fromStage = fromStage->order;
  queueInfo->toStage = toStage->order;
}

void DSWP::collectControlQueueInfo (LoopDependenceInfo *LDI, Parallelization &par) {
  SCCDAG *sccdag = LDI->sccdagAttrs.getSCCDAG();
  std::set<DGNode<Value> *> conditionalBranchNodes;
  std::set<BasicBlock *> loopExits(LDI->loopExitBlocks.begin(), LDI->loopExitBlocks.end());

  for (auto sccNode : sccdag->getNodes()) {
    auto scc = sccNode->getT();

    for (auto controlEdge : scc->getEdges()) {
      if (!controlEdge->isControlDependence()) continue;

      auto controlNode = controlEdge->getOutgoingNode();
      auto controlSCC = sccdag->sccOfValue(controlNode->getT());
      if (LDI->sccdagAttrs.getSCCAttrs(controlSCC)->canBeCloned()) continue;

      conditionalBranchNodes.insert(controlNode);
    }
  }

  for (auto conditionalBranchNode : conditionalBranchNodes) {

    /*
     * Identify the single condition for this conditional branch
     */
    std::set<Instruction *> conditionsOfConditionalBranch;
    for (auto conditionToBranchDependency : conditionalBranchNode->getIncomingEdges()) {
      assert(!conditionToBranchDependency->isMemoryDependence()
        && "DSWP requires that no memory dependencies exist across tasks!");
      if (conditionToBranchDependency->isControlDependence()) continue;

      auto condition = conditionToBranchDependency->getOutgoingT();
      auto conditionSCC = sccdag->sccOfValue(condition);
      if (LDI->sccdagAttrs.getSCCAttrs(conditionSCC)->canBeCloned()) continue;

      conditionsOfConditionalBranch.insert(cast<Instruction>(condition));
    }
    assert(conditionsOfConditionalBranch.size() == 1);

    auto conditionalBranch = cast<Instruction>(conditionalBranchNode->getT());
    assert(LDI->loopBBtoPD.find(conditionalBranch->getParent()) != LDI->loopBBtoPD.end());
    // !LDI->liSummary.getLoop(LDI->loopBBtoPD[conditionalBranch->getParent()]);
    bool isControllingLoopExit = loopExits.find(conditionalBranch->getParent()) != loopExits.end();

    /*
     * Determine which tasks are control dependent on the conditional branch
     */
    std::set<Task *> tasksControlledByCondition;
    if (isControllingLoopExit) {
      tasksControlledByCondition = std::set<Task *>(this->tasks.begin(), this->tasks.end());
    } else {
      tasksControlledByCondition = collectTransitivelyControlledTasks(LDI, conditionalBranchNode);
    }

    /*
     * For each task, add a queue from the condition to the branch
     */
    auto taskOfCondition = this->sccToStage.at(sccdag->sccOfValue(conditionalBranch));
    for (auto techniqueTask : tasksControlledByCondition) {
      auto taskControlledByCondition = (DSWPTask *)techniqueTask;
      if (taskOfCondition == taskControlledByCondition) continue;

      for (auto condition : conditionsOfConditionalBranch) {
        registerQueue(par, LDI, taskOfCondition, taskControlledByCondition, condition, conditionalBranch);
      }
    }
  }
}

std::set<Task *> DSWP::collectTransitivelyControlledTasks (
  LoopDependenceInfo *LDI,
  DGNode<Value> *conditionalBranchNode
) {
  std::set<Task *> tasksControlledByCondition;
  SCCDAG *sccdag = LDI->sccdagAttrs.getSCCDAG();
  auto getTaskOfNode = [this, sccdag](DGNode<Value> *node) {
    return this->sccToStage.at(sccdag->sccOfValue(node->getT()));
  };

  /*
   * To prevent cyclical traversal within a single SCC, include self as controlled
   * The task of the conditional branch is removed after all transitively controlled tasks are added
   */
  Task *selfTask = getTaskOfNode(conditionalBranchNode);
  tasksControlledByCondition.insert(selfTask);

  std::queue<DGNode<Value> *> queuedNodes;
  queuedNodes.push(conditionalBranchNode);

  while (!queuedNodes.empty()) {
    auto node = queuedNodes.front();
    queuedNodes.pop();

    /*
     * Iterate the next set of dependent instructions and collect their tasks
     * Enqueue dependent instructions in tasks not already visited
     */
    for (auto dependencyEdge : node->getOutgoingEdges()) {
      auto dependentNode = dependencyEdge->getIncomingNode();
      Task *dependentTask = getTaskOfNode(dependentNode);
      if (tasksControlledByCondition.find(dependentTask) != tasksControlledByCondition.end()) continue;

      tasksControlledByCondition.insert(dependentTask);
      queuedNodes.push(dependentNode);
    }
  }

  tasksControlledByCondition.erase(selfTask);
  return tasksControlledByCondition;
}

void DSWP::collectDataQueueInfo (LoopDependenceInfo *LDI, Parallelization &par) {
  for (auto techniqueTask : this->tasks) {
    auto toStage = (DSWPTask *)techniqueTask;
    std::set<SCC *> allSCCs(toStage->removableSCCs.begin(), toStage->removableSCCs.end());
    allSCCs.insert(toStage->stageSCCs.begin(), toStage->stageSCCs.end());
    for (auto scc : allSCCs) {
      for (auto sccEdge : LDI->sccdagAttrs.getSCCDAG()->fetchNode(scc)->getIncomingEdges()) {
        auto fromSCC = sccEdge->getOutgoingT();
        auto fromSCCInfo = LDI->sccdagAttrs.getSCCAttrs(fromSCC);
        if (fromSCCInfo->canBeCloned()) {
          continue;
        }
        auto fromStage = this->sccToStage[fromSCC];
        if (fromStage == toStage) continue;

        /*
         * Create value queues for each dependency of the form: producer -> consumers
         */
        for (auto instructionEdge : sccEdge->getSubEdges()) {
          assert(!instructionEdge->isMemoryDependence()
            && "DSWP requires that no memory dependencies exist across tasks!");
          if (instructionEdge->isControlDependence()) continue;
          auto producer = cast<Instruction>(instructionEdge->getOutgoingT());
          auto consumer = cast<Instruction>(instructionEdge->getIncomingT());
          registerQueue(par, LDI, fromStage, toStage, producer, consumer);
        }
      }
    }
  }
}

bool DSWP::areQueuesAcyclical () const {

  /*
   * For each of the ordered vector of tasks:
   * 1) ensure that push queues do not loop back to a previous task
   * 2) ensure that pop queues do not loop forward to a following task
   */
  for (int i = 0; i < this->tasks.size(); ++i) {
    DSWPTask *task = (DSWPTask *)this->tasks[i];

    for (auto queueIdx : task->pushValueQueues) {
      int toTaskIdx = this->queues[queueIdx]->toStage;
      if (toTaskIdx <= i) {
        errs() << "DSWP:  ERROR! Push queue " << queueIdx << " loops back from stage "
          << i << " to stage " << toTaskIdx;
        return false;
      }
    }

    for (auto queueIdx : task->popValueQueues) {
      int fromTaskIdx = this->queues[queueIdx]->fromStage;
      if (fromTaskIdx >= i) {
        errs() << "DSWP:  ERROR! Pop queue " << queueIdx << " goes from stage "
          << fromTaskIdx << " to stage " << i;
        return false;
      }
    }
  }

  return true;
}

void DSWP::generateLoadsOfQueuePointers (
  Parallelization &par,
  int taskIndex
) {
  auto task = (DSWPTask *)this->tasks[taskIndex];
  IRBuilder<> entryBuilder(task->entryBlock);
  auto queuesArray = entryBuilder.CreateBitCast(
    task->queueArg,
    PointerType::getUnqual(this->queueArrayType)
  );

  /*
   * Load this stage's relevant queues
   */
  auto loadQueuePtrFromIndex = [&](int queueIndex) -> void {
    auto queueInfo = this->queues[queueIndex].get();
    auto queueIndexValue = cast<Value>(ConstantInt::get(par.int64, queueIndex));
    auto queuePtr = entryBuilder.CreateInBoundsGEP(queuesArray, ArrayRef<Value*>({
      this->zeroIndexForBaseArray,
      queueIndexValue
    }));
    auto parQueueIndex = par.queues.queueSizeToIndex[queueInfo->bitLength];
    auto queueType = par.queues.queueTypes[parQueueIndex];
    auto queueElemType = par.queues.queueElementTypes[parQueueIndex];
    auto queueCast = entryBuilder.CreateBitCast(queuePtr, PointerType::getUnqual(queueType));

    auto queueInstrs = std::make_unique<QueueInstrs>();
    queueInstrs->queuePtr = entryBuilder.CreateLoad(queueCast);
    queueInstrs->alloca = entryBuilder.CreateAlloca(queueInfo->dependentType);
    queueInstrs->allocaCast = entryBuilder.CreateBitCast(
      queueInstrs->alloca,
      PointerType::getUnqual(queueElemType)
    );
    task->queueInstrMap[queueIndex] = std::move(queueInstrs);
  };

  for (auto queueIndex : task->pushValueQueues) loadQueuePtrFromIndex(queueIndex);
  for (auto queueIndex : task->popValueQueues) loadQueuePtrFromIndex(queueIndex);
}

void DSWP::popValueQueues (Parallelization &par, int taskIndex) {
  auto task = (DSWPTask *)this->tasks[taskIndex];
  auto &bbClones = task->basicBlockClones;
  auto &iClones = task->instructionClones;

  for (auto queueIndex : task->popValueQueues) {
    auto &queueInfo = this->queues[queueIndex];
    auto queueInstrs = task->queueInstrMap[queueIndex].get();
    auto queueCallArgs = ArrayRef<Value*>({ queueInstrs->queuePtr, queueInstrs->allocaCast });

    auto bb = queueInfo->producer->getParent();
    assert(bbClones.find(bb) != bbClones.end());

    IRBuilder<> builder(bbClones[bb]);
    auto queuePopFunction = par.queues.queuePops[par.queues.queueSizeToIndex[queueInfo->bitLength]];
    queueInstrs->queueCall = builder.CreateCall(queuePopFunction, queueCallArgs);
    queueInstrs->load = builder.CreateLoad(queueInstrs->alloca);

    /*
     * Map from producer to queue load 
     */
    task->instructionClones[queueInfo->producer] = (Instruction *)queueInstrs->load;

    /*
     * Position queue call and load relatively identically to where the producer is in the basic block
     */
    bool pastProducer = false;
    bool moved = false;
    for (auto &I : *bb) {
      if (&I == queueInfo->producer) pastProducer = true;
      else if (auto phi = dyn_cast<PHINode>(&I)) continue;
      else if (pastProducer && iClones.find(&I) != iClones.end()) {
        auto iClone = iClones[&I];
        cast<Instruction>(queueInstrs->queueCall)->moveBefore(iClone);
        cast<Instruction>(queueInstrs->load)->moveBefore(iClone);
        moved = true;
        break;
      }
    }
    assert(moved);
  }
}

void DSWP::pushValueQueues (Parallelization &par, int taskIndex) {
  auto task = (DSWPTask *)this->tasks[taskIndex];
  auto &iClones = task->instructionClones;

  for (auto queueIndex : task->pushValueQueues) {
    auto queueInstrs = task->queueInstrMap[queueIndex].get();
    auto queueInfo = this->queues[queueIndex].get();
    auto queueCallArgs = ArrayRef<Value*>({ queueInstrs->queuePtr, queueInstrs->allocaCast });
    
    auto pClone = iClones[queueInfo->producer];
    auto pCloneBB = pClone->getParent();
    IRBuilder<> builder(pCloneBB);
    auto store = builder.CreateStore(pClone, queueInstrs->alloca);
    auto queuePushFunction = par.queues.queuePushes[par.queues.queueSizeToIndex[queueInfo->bitLength]];
    queueInstrs->queueCall = builder.CreateCall(queuePushFunction, queueCallArgs);

    bool pastProducer = false;
    for (auto &I : *pCloneBB) {
      if (&I == pClone) pastProducer = true;
      else if (auto phi = dyn_cast<PHINode>(&I)) continue;
      else if (pastProducer) {
        store->moveBefore(&I);
        cast<Instruction>(queueInstrs->queueCall)->moveBefore(&I);
        break;
      }
    }
  }
}

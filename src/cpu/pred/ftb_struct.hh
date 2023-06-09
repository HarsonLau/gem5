#ifndef __FTB_STRUCT_HH__
#define __FTB_STRUCT_HH__
#include "arch/generic/pcstate.hh"
#include "cpu/inst_seq.hh"
#include "cpu/o3/limits.hh"
#include "cpu/static_inst.hh"

namespace gem5
{
    namespace branch_prediction
    {
        typedef uint64_t FetchStreamIdType;
        typedef uint64_t FetchTargetIdType;
        typedef uint8_t InstSizeType;
        typedef int BlockSizeType;

        using DynInstPtr = gem5::o3::DynInstPtr;

        enum class BranchType
        {
            Unconditional,
            Conditional,
            Indirect,
            Call,
            Return,
            Syscall,
            Sysret,
            Other
        };

        /**
         * @brief The FTQ Entry class
         * This class describes a single entry in the Fetch Target Queue.
         * The FTQ is a buffer between the Stream Queue and the I-Cache.
         * The FTQ entry should be readonly.
         */
        struct FTQEntry
        {
        public:
            /**
             * @brief  whether this entry terminates a stream
             */
            const bool isStreamEnd;

            /**
             * @brief stream id of this entry; read only
             */
            const FetchStreamIdType streamId;

            // [startPC, endPC) for now
            const Addr startPC;
            const Addr endPC;
            const BlockSizeType size;
            // do we need this?
            const bool hasBranch;
            const Addr targetPC;

            FTQEntry(FetchStreamIdType stream_id, Addr start_pc, Addr end_pc,
                     bool has_branch, Addr target_pc, bool is_stream_end)
                : streamId(stream_id), startPC(start_pc), endPC(end_pc),
                  hasBranch(has_branch), targetPC(target_pc),
                  isStreamEnd(is_stream_end), size(end_pc - start_pc)
            {
            }
            ~FTQEntry() {}
        };

        struct BranchInfo
        {
        public:
            Addr pc;
            Addr target;
            InstSizeType size;
            BranchType type;

            BranchInfo() : pc(0), target(0), size(0), type(BranchType::Other)
            {
            }

            /**
             * @brief Construct a new Branch Info object
             * @param control_pc the PC of the branch instruction
             * @param target_pc the target PC of the branch instruction
             * @param inst the static branch instruction
             * @param size the size of the branch instruction
             */
            BranchInfo(const Addr &control_pc, const Addr &target_pc,
                       const StaticInstPtr &inst, InstSizeType size)
                : pc(control_pc), target(target_pc), size(size),
                  type(BranchType::Other)
            {

                if (inst->isCondCtrl())
                {
                    type = BranchType::Conditional;
                }
                else if (inst->isIndirectCtrl())
                {
                    type = BranchType::Indirect;
                }
                else if (inst->isUncondCtrl())
                {
                    type = BranchType::Unconditional;
                }
                else if (inst->isCall())
                {
                    type = BranchType::Call;
                }
                else if (inst->isReturn() && !inst->isNonSpeculative() &&
                         !inst->isDirectCtrl())
                {
                    type = BranchType::Return;
                }
                // TODO: support other type of branches
            }

            // BranchInfo(const Addr &control_pc, const Addr &target_pc,
            //            const DynInstPtr &inst, InstSizeType size):
            //     BranchInfo(control_pc, target_pc, inst->staticInst, size)
            // {
            // }

            bool isUnconditional() const
            {
                return type == BranchType::Unconditional;
            }
            bool isConditional() const
            {
                return type == BranchType::Conditional;
            }
            bool isIndirect() const { return type == BranchType::Indirect; }
            bool isCall() const { return type == BranchType::Call; }
            bool isReturn() const { return type == BranchType::Return; }
            bool isSyscall() const { return type == BranchType::Syscall; }
            bool isSysret() const { return type == BranchType::Sysret; }
            bool isOther() const { return type == BranchType::Other; }

            bool operator<(const BranchInfo &other) const
            {
                return this->pc < other.pc;
            }

            bool operator==(const BranchInfo &other) const
            {
                return this->pc == other.pc;
            }

            bool operator>(const BranchInfo &other) const
            {
                return this->pc > other.pc;
            }

            bool operator!=(const BranchInfo &other) const
            {
                return this->pc != other.pc;
            }
        };

        class BranchSlot : public BranchInfo
        {
        public:
            bool valid;
            bool alwaysTaken;

            /**
             * @brief Check if the branch is valid and conditional
             */
            bool condValid() { return valid && this->isConditional(); }

            /**
             * @brief Check if the branch is valid and unconditional
             */
            bool uncondValid() { return valid && this->isUnconditional(); }

            BranchSlot() : BranchInfo(), valid(false), alwaysTaken(false) {}
            /**
             * @brief Construct a new Branch Slot object
             * @param bi the branch info
             */
            BranchSlot(const BranchInfo &bi)
                : BranchInfo(bi), valid(true), alwaysTaken(true)
            {
            }
            ~BranchSlot() {}

            void invalidate() { valid = false; }
            void validate() { valid = true; }

            void unsetAlwaysTaken() { alwaysTaken = false; }

            bool operator==(const BranchSlot &other) const
            {
                return this->valid == other.valid && this->pc == other.pc &&
                       this->target == other.target &&
                       this->size == other.size && this->type == other.type &&
                       this->alwaysTaken == other.alwaysTaken;
            }
        };

        /**
         * @brief The FTB Entry class template.
         * This class describes a single entry in the Fetch Target Buffer.
         */
        template <int SlotNum> class FTBEntryBase
        {
        public:
            std::vector<BranchSlot> branchSlots;

            /**
             * @brief The tag of this entry.
             */
            Addr tag;

            /**
             * @brief The thread id of this entry.
             */
            ThreadID tid;

            /**
             * @brief The fallthrough PC of this entry.
             * TODO: use partial address
             */
            Addr fallthroughPC;

            /**
             * @brief The valid bit of this entry.
             */
            bool valid;

            ~FTBEntryBase() {}
            FTBEntryBase() : fallthroughPC(0), valid(false) {}

            FTBEntryBase(const FTBEntryBase &ftb_entry)
            {
                tag = ftb_entry.tag;
                tid = ftb_entry.tid;
                fallthroughPC = ftb_entry.fallthroughPC;
                valid = ftb_entry.valid;
                branchSlots.resize(ftb_entry.branchSlots.size());
                for (int i = 0; i < branchSlots.size(); i++)
                {
                    branchSlots[i] = ftb_entry.branchSlots[i];
                }
            }

            /**
             * @retval Returns the number of valid branch slots in this entry.
             */
            int validSlotNum() const
            {
                int cnt = 0;
                for (int i = 0; i < branchSlots.size(); i++)
                {
                    if (branchSlots[i].valid)
                    {
                        cnt++;
                    }
                }
                return cnt;
            }

            /**
             * @retval Returns the number of valid branch slots before the
             * given pc in this entry.
             */
            int validSlotNumBefore(Addr pc) const
            {
                int cnt = 0;
                for (int i = 0; i < branchSlots.size(); i++)
                {
                    if (branchSlots[i].valid && branchSlots[i].pc < pc)
                    {
                        cnt++;
                    }
                }
                return cnt;
            }

            /**
             * @retval Returns the branch slot index of the given pc in this
             * entry. If the pc is not found, returns -1.
             */
            int slotIdx(Addr pc) const
            {
                for (int i = 0; i < branchSlots.size(); i++)
                {
                    if (branchSlots[i].valid && branchSlots[i].pc == pc)
                    {
                        return i;
                    }
                }
                return -1;
            }

            /**
             * @brief Checks whether this entry is reasonable.
             * @retval Returns true if all the branch slots and the fallthrough
             * pc locates in (start, start + 34].
             */
            bool isReasonable(Addr start)
            {
                auto min = start;
                auto max = start + 34;
                bool reasonable = true;
                for (auto &slot : branchSlots)
                {
                    if (slot.valid && (slot.pc < min || slot.pc > max))
                    {
                        reasonable = false;
                        break;
                    }
                }
                if (fallthroughPC < min || fallthroughPC > max)
                {
                    reasonable = false;
                }
                return reasonable;
            }

            bool operator==(const FTBEntryBase &other) const
            {
                if (fallthroughPC != other.fallthroughPC)
                {
                    return false;
                }
                if (valid != other.valid)
                {
                    return false;
                }
                if (branchSlots.size() != other.branchSlots.size())
                {
                    return false;
                }
                for (int i = 0; i < branchSlots.size(); i++)
                {
                    if (branchSlots[i] != other.branchSlots[i])
                    {
                        return false;
                    }
                }
                return true;
            }
        };

        // ftb entry with dynamic prediction and execution information
        template <int SlotNum> class FetchStream
        {
        protected:
            // seems like prediction information is for internal use only
            using FTBEntry = FTBEntryBase<SlotNum>;
            FTBEntry _predEntry;
            FTBEntry _updatedEntry;

            BranchInfo predBranchInfo;
            bool predHasBranch;
            bool predHasTakenBranch;
            Addr predTakenBranchPC;
            Addr predTargetBranchPC;
            /**
             * @brief The fallthrough pc of the stream. Equvalent to predEndPC
             * in XiangShan
             */
            Addr predFallthroughPC;

            bool resolved;
            // maybe we need these information from outside
            BranchInfo execBranchInfo;
            bool _execHasBranch;
            bool _execHasTakenBranch;
            Addr _execTakenBranchPC;
            Addr _execTargetBranchPC;
            Addr _execFallthroughPC;

            InstSeqNum _firstInstSeq;
            InstSeqNum _lastInstSeq;

            // tracking the last committed instruction to know when the
            // execution of this stream has finished
            InstSeqNum lastCommittedInstSeq;

            void condCtrlCommitted(DynInstPtr inst)
            {
                assert(inst->isCondCtrl());

                auto taken = inst->pcState().branching();

                if (taken)
                {
                }
            }

            void uncondCtrlCommitted(DynInstPtr inst) {}

        public:
            // TODO: is this necessary?
            const FetchStreamIdType streamId;

        public:
            // TODO: interface to inform the ftb entry that a new instruction
            // has been fetched
            void newInstFetched(InstSeqNum inst_seq)
            {
                if (_firstInstSeq == 0)
                {
                    _firstInstSeq = inst_seq;
                }
                _lastInstSeq = inst_seq;
            }

            // TODO: interface to inform the ftb entry that a new instruction
            // has been committed
            void newInstCommitted(DynInstPtr inst, InstSeqNum inst_seq)
            {
                assert(inst_seq == 0 || inst_seq > lastCommittedInstSeq);
                lastCommittedInstSeq = inst_seq;

                if (inst->isCondCtrl())
                {
                    condCtrlCommitted(inst);
                }
                else if (inst->isUncondCtrl())
                {
                    uncondCtrlCommitted(inst);
                }
                else
                {
                }
            }

            bool taken() const
            {
                return resolved ? _execHasTakenBranch : predHasTakenBranch;
            }

            BranchInfo getBranchInfo() const
            {
                return resolved ? execBranchInfo : predBranchInfo;
            }

            Addr getControlPC() const { return getBranchInfo().pc; }

            bool isTaken() const
            {
                return resolved ? _execHasTakenBranch : predHasTakenBranch;
            }

            Addr getTakenTarget() const { return getBranchInfo().target; }

            Addr takenBranchPC() const
            {
                return resolved ? _execTakenBranchPC : predTakenBranchPC;
            }

            Addr targetPC() const
            {
                return resolved ? _execTargetBranchPC : predTargetBranchPC;
            }

            // TODO: When unexpected branch is detected, we need to divide the
            // current FTBEntry into two FTBEntries. The first one is the
            // current FTBEntry, and the second one is the new FTBEntry. We
            // need to write these two FTBEntries into the FTB.

        protected:
            /**
             * @brief commit the FetchStream ?
             * TODO: what does this function do?
             */
            void commit() {}

        public:
            // getters for private members with underscore
            bool execHasBranch() const { return _execHasBranch; }
            bool execHasTakenBranch() const { return _execHasTakenBranch; }
            Addr execTakenBranchPC() const { return _execTakenBranchPC; }
            Addr execTargetBranchPC() const { return _execTargetBranchPC; }
            Addr execFallthroughPC() const { return _execFallthroughPC; }

            InstSeqNum firstInstSeq() const { return _firstInstSeq; }
            InstSeqNum lastInstSeq() const { return _lastInstSeq; }

            bool finished() const
            {
                return lastCommittedInstSeq == _lastInstSeq;
            }
        };

    }
}

#endif // __FTB_STRUCT_HH__

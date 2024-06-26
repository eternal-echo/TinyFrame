#include "TinyFrame.h"

#include <string.h>

#include "easylink/EasyLink.h"
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Mailbox.h>

Semaphore_Struct TF_semStruct;
Semaphore_Handle TF_semHandle;
extern Mailbox_Handle tf_tx_mailbox_handle;

/**
 * This is an example of integrating TinyFrame into the application.
 * 
 * TF_WriteImpl() is required, the mutex functions are weak and can
 * be removed if not used. They are called from all TF_Send/Respond functions.
 * 
 * Also remember to periodically call TF_Tick() if you wish to use the 
 * listener timeout feature.
 */
void TF_WriteImpl(TinyFrame *tf, const uint8_t *buff, uint32_t len)
{
    EasyLink_TxPacket txPacket = {0};
    EasyLink_Status result;
    txPacket.len = len;
    memcpy(txPacket.payload, buff, len);
    txPacket.dstAddr[0] = 0xaa; // 设置目标地址，可以根据需要修改
    txPacket.absTime = 0; // 设置绝对时间，0表示立即发送
    
    // 通过邮箱发送数据包到TF_process_func进行处理
    if (!Mailbox_post(tf_tx_mailbox_handle, &txPacket, BIOS_NO_WAIT)) {
        TF_Error("[TF][hw] Mailbox_post failed");
    }
}

// --------- Mutex callbacks ----------
// Needed only if TF_USE_MUTEX is 1 in the config file.
// DELETE if mutex is not used

/** Claim the TX interface before composing and sending a frame */
bool TF_ClaimTx(TinyFrame *tf)
{
    if (Semaphore_pend(TF_semHandle, 0) == TRUE) {
        return true;
    }
    return false;
}

/** Free the TX interface after composing and sending a frame */
void TF_ReleaseTx(TinyFrame *tf)
{
    Semaphore_post(TF_semHandle);
}

// --------- Custom checksums ---------
// This should be defined here only if a custom checksum type is used.
// DELETE those if you use one of the built-in checksum types

/** Initialize a checksum */
TF_CKSUM TF_CksumStart(void)
{
    return 0;
}

/** Update a checksum with a byte */
TF_CKSUM TF_CksumAdd(TF_CKSUM cksum, uint8_t byte)
{
    return cksum ^ byte;
}

/** Finalize the checksum calculation */
TF_CKSUM TF_CksumEnd(TF_CKSUM cksum)
{
    return cksum;
}

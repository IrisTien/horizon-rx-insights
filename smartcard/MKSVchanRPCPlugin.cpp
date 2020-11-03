/* **************************************************************************** *
 * Copyright (C) 2014-2020 VMware, Inc.  All rights reserved. -- VMware Confidential *
 * **************************************************************************** */

/*
 * MKSVchanRPCPlugin.cpp --
 *
 */

#include "MKSVchanRPCPlugin.h"
#include "fileCopyUtils.h"
#include "filetransfer/fileTransfer.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <streambuf>


/*
 * We don't need to wait for the client to connect.
 * We only initialize the required vdp service callbacks using RPCManager
 * and return.
 */
#define RPC_INIT_TIMEOUT_MS 0

MKSVchanRPCManager  mksvchanRPCManager;

typedef std::list< MKSVchanPacketType > RegisteredOnDonePacketTypeList;
RegisteredOnDonePacketTypeList onDonePacketTypeList;

typedef std::list< MKSVchanPacketType > RegisteredOnInvokePacketTypeList;
RegisteredOnInvokePacketTypeList onInvokePacketTypeList;

/*
 * Create the five global entry points required by vdp service
 */
#ifdef RDE_MOBILE_LIBS
VDP_SERVICE_CREATE_INTERFACE(MKSVCHAN_TOKEN_NAME, mksvchanRPCManager, CLIPBOARD)
VDP_SERVICE_MOBILE_PLUGIN_INIT(CLIPBOARD)
#else
VDP_SERVICE_CREATE_INTERFACE(MKSVCHAN_TOKEN_NAME, mksvchanRPCManager)
#endif

#define CLIPBOARD_DATA_PARM_NAME "Clipboard data"
#define CLIPBOARD_ERROR_PARM_NAME "Clipboard error"
#define CLIPBOARD_PARM_MAXLEN 1024

/*
 *----------------------------------------------------------------------------
 *
 * class MKSVchanRPCManager --
 *
 *----------------------------------------------------------------------------
 */

/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCManager::MKSVchanRPCManager --
 *
 *   MKSVchanRPCManager constructor
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

MKSVchanRPCManager::MKSVchanRPCManager()
   : RPCManager(MKSVCHAN_TOKEN_NAME,
                MKSVCHAN_CONTROL_OBJ_NAME,
                MKSVCHAN_DATA_OBJ_NAME,
                MKSVCHAN_DATA_OBJ_TCP_NAME),
     m_MKSVchanRPCPluginInstance(NULL),
     m_LoggerInitializedByRpcManager(FALSE),
     m_pcoipInitCalled(FALSE)
{

}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCManager::OnCreateInstance --
 *
 *   Called by RPCManager on the client side.
 *
 * Results:
 *    Creates a new instance of m_MKSVchanRPCPluginInstance
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

RPCPluginInstance*
MKSVchanRPCManager::OnCreateInstance()
{
   Log("%s: Request for MKSVchan plugin to be created.\n", __FUNCTION__);

   if (IsClient()) {
      if (!MKSVchanPlugin_Init(TRUE, NULL)) {
         Log("%s: Call to MKSVchanPlugin_Init failed. Unable to create MKSVchanRPCPlugin\n",
             __FUNCTION__);
         return NULL;
      }
   }

   /*
    * Note: this instance is deleted by RPCManager OnDestroyInstance.
    * We only take the responsibility of creating the instance.
    * Thereafter, RPCManager takes care of deleting it.
    */
   m_MKSVchanRPCPluginInstance = new MKSVchanRPCPlugin(this, mDnDMsgHandler,
                                                       mFcpMsgHandler);
   return m_MKSVchanRPCPluginInstance;
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCManager::OnDestroyInstance --
 *
 *   Called by RPCManager on the client side.
 *
 * Results:
 *    Deletes the plugin instance and stops the helper thread
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCManager::OnDestroyInstance(RPCPluginInstance *pluginInstance) //IN
{
   ASSERT(pluginInstance == m_MKSVchanRPCPluginInstance);
   Log("%s: Request for MKSVchan plugin to be destroyed.\n", __FUNCTION__);

   if (IsClient()) {
      MKSVchanPlugin_Cleanup(TRUE, TRUE);
   }

   if (m_MKSVchanRPCPluginInstance != NULL) {
      delete m_MKSVchanRPCPluginInstance;
      m_MKSVchanRPCPluginInstance = NULL;
      Log("%s: MKSVchan plugin instance has been destroyed.\n", __FUNCTION__);
   }
   m_pcoipInitCalled = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCManager::Init --
 *
 *    Invoked by the MKSVchan server to create plugin instance
 *    and initialize RPC server
 *
 * Results:
 *    True if initialization was successful.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
MKSVchanRPCManager::Init(Bool isServer,                 // IN
                         BaseMsgHandler *dndMsgHandler, // IN
                         BaseMsgHandler *fcpMsgHandler) // IN
{
   if (isServer) {
      m_MKSVchanRPCPluginInstance = new MKSVchanRPCPlugin(this, dndMsgHandler,
                                                          fcpMsgHandler);

      // Call RPCManager's ServerInit
      return ServerInit(m_MKSVchanRPCPluginInstance, RPC_INIT_TIMEOUT_MS);
   } else {
      mDnDMsgHandler = dndMsgHandler;
      mFcpMsgHandler = fcpMsgHandler;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCManager::Exit --
 *
 *    Called by the MKSVchan server on exiting.
 *    It deletes the plugin instance and calls ServerExit
 *
 * Results:
 *    True if exit was successful.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
MKSVchanRPCManager::Exit()
{
   if (NULL == m_MKSVchanRPCPluginInstance) {
      Log("%s: No cleanup needed. Exiting.\n", __FUNCTION__);
      return TRUE;
   }

   // For server, plugininstance is created during Init. For client, it is destroyed by the RPCManager OnDestroyInstance
   if (IsServer()) {
      if (!ServerExit(m_MKSVchanRPCPluginInstance)) {
         Log("%s: ServerExit failed.\n", __FUNCTION__);
         return FALSE;
      }

      // TODO: do the DnD exit when client code is ready
      // m_MKSVchanRPCPluginInstance->ExitDnD();
      delete m_MKSVchanRPCPluginInstance;
      m_MKSVchanRPCPluginInstance = NULL;
   }

    return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCManager::OnClientInit --
 *
 *    Called by the RPCManager when the client plugin has been initialized
 *
 * Results:
 *    Start mksvchan logging for vvc
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCManager::OnClientInit()
{
#if defined(RDELIBS)
   m_LoggerInitializedByRpcManager = !MKSVchan_IsLoggerInitialized();
   if (m_LoggerInitializedByRpcManager) {
      if (MKSVchan_StartLogging(MKSVCHAN_CLIENT_LOG_FILENAME, NULL, TRUE)) {
         Log("%s: MKSVchanClient logging initialized successfully.\n", __FUNCTION__);
      }
   }
#endif

   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCManager::OnClientExit --
 *
 *    Called by the RPCManager when the client plugin exits
 *
 * Results:
 *    Stop mksvchan logging in vvc sessions. For PCOIP, this is no-op
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCManager::OnClientExit()
{

#if defined(RDELIBS)
   /* Stop mksvchan logging only if RPCManager started the logging.
    * In cases of pcoip, pcoip exit function will stop the logger.
    */
   if (m_LoggerInitializedByRpcManager) {
      Log("%s: Uninitializing MKSVchanClient logging.\n", __FUNCTION__);
      MKSVchan_StopLogging();
      m_LoggerInitializedByRpcManager = FALSE;
   }
#endif

   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * class MKSVchanRPCPlugin --
 *
 *----------------------------------------------------------------------------
 */

/*
*----------------------------------------------------------------------------
*
* MKSVchanRPCPlugin::MKSVchanRPCPlugin --
*
*   MKSVchanRPCPlugin constructor
*
* Results:
*    None.
*
* Side effects:
*    None.
*
*----------------------------------------------------------------------------
*/

MKSVchanRPCPlugin::MKSVchanRPCPlugin(RPCManager* rpcManagerPtr,     // IN
                                     BaseMsgHandler *dndMsgHandler, // IN
                                     BaseMsgHandler *fcpMsgHandler) // IN
   : RPCPluginInstance(rpcManagerPtr),
     mDnDMsgHandler(NULL),
     mFcpMsgHandler(NULL)
{
   m_chunkSendCount = 0;
#if (defined(_WIN32) && !defined(VM_WIN_UWP)) || TARGET_OS_OSX
   mDnDMsgHandler = dndMsgHandler;
   mFcpMsgHandler = fcpMsgHandler;
   Log("%s: DnD and FCP message handlers are set.\n", __FUNCTION__);
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::OnReady --
 *
 *    Called by the RPCManager when client/server is in ready state
 *
 * Results:
 *    Signals the client/server application that the vdpservice channel is
 *    ready to use. Does other MKSVchanplugin related initiatization such as
 *    set the negotiated capability for the clipboard and set the helper
      thread id to that of the current thread id.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::OnReady()
{
   Log("%s: OnReady called.\n", __FUNCTION__);
   //int result = -1;
   std::stringstream buffer;
   std::string str;
   // Set helper thread as the vdp service thread
   MKSVchan_SetVdpServiceThreadId();

   RPCManager* rpcManager = GetRPCManager();
   ASSERT(rpcManager);

   if (!rpcManager->IsServer()) {
      // tests for windows
#if defined(_WIN32) && !defined(VM_WIN_UWP)
      MKSVchan_SendSmartCardInfo("{\"test\":123}");
#endif


   const char *homeDir = getenv("HOME");
   
   Log("===================================================================Sabrina==================================");

   std::string path = std::string(homeDir) + "/devices.json";
   std::string cmd = std::string("system_profiler SPPrintersDataType SPSmartCardsDataType -json>") + path;
   system(cmd.c_str());
   //std::ifstream t("/Users/shou/devices.json");
   std::ifstream t(path.c_str());
   std::string str((std::istreambuf_iterator<char>(t)),
                    std::istreambuf_iterator<char>());
   Log("Length:%ld", str.length());
   
   Log("%s", str.c_str());
   Log("Debug code for sending smart card info.");
   MKSVchan_SendSmartCardInfo(str.c_str());
  // MKSVchan_SendSmartCardInfo("Just for testing");


   }

#if defined(_WIN32) && !defined(VM_WIN_UWP)
   /*
    * Need to init some ft variables for both client and server
    * NOTE: Init criticalSection here is not always safe. It cannot
    * handle the old pcoip connection. Please refer to bug 1759096.
    * Due to get file list is disabled on client side, file transfer
    * related code will never be called on client side.
    */
   FT::InitFTConfig();
#endif

   if (rpcManager->IsServer()) {
      if (!MKSVchanPlugin_Init(FALSE, NULL)) {
         Log("%s: Unable to initialize mksvchan.\n", __FUNCTION__);
         return;
      }
      Log("%s: Send desired capabilities.\n", __FUNCTION__);
      MKSVchan_QueueClipboardCapability();

      Log("%s: Send clipboard state.\n", __FUNCTION__);
      MKSVchan_QueueClipboardState();

      Log("%s: Send file transfer config.\n", __FUNCTION__);
      MKSVchan_QueueFileTransferConfig();

#if defined(_WIN32) && !defined(VM_WIN_UWP)

      if (NULL != mDnDMsgHandler) {
         /*
          * For DnD server, check whether it's running in a remote desktop
          * session or a remote application session, if the latter, unity
          * mode will be set.
          */
         mDnDMsgHandler->CheckSessionType();

         // Only server will queue, client does nothing.
         Log("%s: Send DnD capabilities.\n", __FUNCTION__);
         mDnDMsgHandler->QueueDnDCapability();
      }
#endif
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::OnNotReady --
 *
 *    Called by the RPCManager when client/server disconnects
 *
 * Results:
 *    Signals the client/server application with the help of an event, that the
 *    vdpservice channel has been disconnected.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::OnNotReady()
{
   // Cleanup
   ASSERT(GetRPCManager());
   if (GetRPCManager()->IsServer()) {
      // If notify on change is enabled, unregister from any clipboard change notification
      if (MKSVchanPlugin_ShouldListenForClipboardChanges()) {
         MKSVchan_QueueUnregisterClipboardListener();
      }
      MKSVchanPlugin_Cleanup(TRUE, FALSE);
#if defined(_WIN32) && !defined(VM_WIN_UWP)
      FT::OnInterrupt(TRUE);
#endif
      m_requestList.clear();
   } else {
#if defined(_WIN32) && !defined(VM_WIN_UWP)
      FT::OnInterrupt(FALSE);
#endif
   }

   // Reset the vdpservice thread id tracked by the plugin
   MKSVchan_ResetVdpServiceThreadId();

   if (NULL != mFcpMsgHandler) {
      Log("%s: Notify Fcp MKSVchan plugin got disconnected.\n", __FUNCTION__);
      mFcpMsgHandler->OnRecvMKSVchanNotReady();
   }

   if (NULL != mDnDMsgHandler) {
      Log("%s: Notify DnD MKSVchan plugin got disconnected.\n", __FUNCTION__);
      mDnDMsgHandler->OnRecvMKSVchanNotReady();
   }

#ifndef VM_WIN_UWP
   if ((nullptr != mDnDMsgHandler) || (nullptr!= mFcpMsgHandler)) {
      // Remove temporary folders which may created by DnD and FCP features
      FileCopyUtils::RemoveTempFolder();
   }
#endif

   Log("%s: MKSVchan plugin got disconnected.\n", __FUNCTION__);
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::OnConnectionReject --
 *
 *    Signals the server that the client rejected the connection.
 *    Server will fallback on pcoip.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::OnConnectionReject()
{
   RPCManager* rpcManager = GetRPCManager();
   ASSERT(rpcManager);

   Log("%s: Vdp service open request rejected by the %s.\n", __FUNCTION__, rpcManager->IsServer() ? "client" : "server");

   Log("%s: Cleaning up mksvchan plugin state.\n", __FUNCTION__);
   MKSVchanPlugin_Cleanup(FALSE, FALSE);

   if (rpcManager->IsServer()) {
      Log("%s: Signaling the server to register for pcoip connections instead.\n", __FUNCTION__);
      if (!MKSVchanRPCWrapper_SetVMEvent(VDP_CHANNEL_OPEN_REJECTED_EVENT_NAME)) {
         Log("%s: Unable to signal the mksvchan server that the vdp open request was "
            "rejected. Reason: Unable to set the event.\n", __FUNCTION__);
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::OnDone --
 *
 *    Called by the RPCManager after the message has been received
 *
 * Results:
 *    Not implemented
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::OnDone(uint32 requestCtxId, // IN
                          void *returnCtx)     // IN
{
   // Log the total round trip time to send the clipboard data
   MKSVchanRPCPlugin::MKSVchanCPRequestList::iterator it = m_requestList.begin();
   while (it != m_requestList.end()) {
      if (it->m_id == requestCtxId) {
         if (it->m_dataType == MKSVchanCPRequest::MKS_FileTransfer_Data) {
#if defined(_WIN32) && !defined(VM_WIN_UWP)
            // Only windows implementation for file transfer now
            m_chunkSendCount++;
            if (m_chunkSendCount == FT::GetCurrentNumberOfChunkToSend()) {
               m_chunkSendCount = 0;
               FT::SendNextFileChunks();
            }
#endif
         } else if (it->m_dataType == MKSVchanCPRequest::MKS_DropInteraction_Data) {
            if (it->m_onDoneHandler) {
               it->m_onDoneHandler(MKSVchanPacketType_LegacyDnD_Data);
            }
            Log("%s: Sending drop interaction data of %u-bytes "
                "payload took %dms\n",
                __FUNCTION__, it->m_dataLen, it->m_timer.MarkMS());
         } else {
            Log("%s: Sending %u-bytes payload took %dms\n",
                __FUNCTION__, it->m_dataLen, it->m_timer.MarkMS());
            NotifyForRegisteredOnDonePacketType(it);
         }

         m_requestList.erase(it);
         break;
      }
      ++it;
   }
   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::OnAbort --
 *
 *    Called by the RPCManager in case the message was discarded by the other side
 *
 * Results:
 *    Not implemented
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::OnAbort(uint32 requestCtxId, // IN
                           Bool userCancelled,  // IN
                           uint32 reason)       // IN
{
   // Not implemented
   return;
}


/*
 *----------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::isDataValid --
 *
 *    Check whether the package received from peer contains valid data or not.
 *
 * Results:
 *    TRUE if data is valid, otherwise FALSE.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
MKSVchanRPCPlugin::isDataValid(RPCVariant *data, // OUT
                               void *messageCtx) // IN
{
   const VDPRPC_ChannelContextInterface *iChannelCtx = ChannelContextInterface();
   char paramName[CLIPBOARD_PARM_MAXLEN];

   if (!iChannelCtx->v1.GetNamedParam(messageCtx, 0, paramName,
                                      CLIPBOARD_PARM_MAXLEN, data)) {
      Log("%s: Could not retrieve variant at parameter 0\n", __FUNCTION__);
      return FALSE;
   }

   /* we still have to keep the blob check for backward compatibility */
   if (strcmp(paramName, CLIPBOARD_DATA_PARM_NAME) &&
       data->vt != VDP_RPC_VT_BLOB) {
      Log("%s: Error - No data found at param 0.\n", __FUNCTION__);
      return FALSE;
   }

   return TRUE;
}


/*
 *---------------------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::OnInvoke --
 *
 *    Called by the RPCManager when the plugin receives any message from the other side
 *    Opens up the message and figures out the command type (request or send clipboard data)
 *          1. For request, depending on the policy set, it calls the mksvchan plugin API
 *             to send the clipboard data
 *          2. For send clipboard data packet type, depending on the policy set, it calls the
 *             mksvchan API to set the clipboard with the received data blob.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::OnInvoke(void* messageCtx) // IN: message context from vdpservice
{
   const VDPRPC_ChannelContextInterface* iChannelCtx = ChannelContextInterface();

   // Get the packet type
   MKSVchanPacketType receivedPacketType = (MKSVchanPacketType)iChannelCtx->v1.GetCommand(messageCtx);

   Log("%s: Received packetType = %s.\n", __FUNCTION__,
       GetMKSVchanPacketTypeAsString(receivedPacketType));

   /*
    * Now take appropriate action
    * For any clipboard request, send the clipboard data
    */
   switch (receivedPacketType) {
      case MKSVchanPacketType_Clipboard_Locale:
      {
         // Set the locale
         RPCVariant langIdData(this);

         if (!isDataValid(&langIdData, messageCtx)) {
            return;
         }

         uint32* langIdValuePtr = reinterpret_cast<uint32*>(langIdData.blobVal.blobData);
         Log("%s: Received locale, langid = 0x%08x.\n", __FUNCTION__, *langIdValuePtr);
         MKSVchanPlugin_SetClipboardLocale(*langIdValuePtr);
      }
      break;

      case MKSVchanPacketType_Clipboard_Capabilities:
      {
         // Extract capability value
         RPCVariant capsData(this);

         if (!isDataValid(&capsData, messageCtx)) {
            return;
         }

         uint32* capsValuePtr = reinterpret_cast<uint32*>(capsData.blobVal.blobData);
         Log("%s: Received capability value 0x%08x.\n", __FUNCTION__, *capsValuePtr);
         MKSVchanPlugin_SetClipboardCaps(*capsValuePtr);
      }
      break;

      case MKSVchanPacketType_ClipboardPasteNotification:
      {
         RPCVariant aduitLog(this);

         if (!isDataValid(&aduitLog, messageCtx)) {
            return;
         }

         Log("%s: Received audit message of size %d.\n", __FUNCTION__,
             aduitLog.blobVal.size);
         uint8* data = reinterpret_cast<uint8*>(aduitLog.blobVal.blobData);
         //if (MKSVchanPlugin_IsClipboardAuditEnabled()) {
            MKSVchanPlugin_SetClipboardAudit(data,
                                             aduitLog.blobVal.size);
         //}

         //if (MKSVchanPlugin_IsUserOperationDataNeeded()) {
         //   MKSVchanPlugin_SendUserOpMetricFromStream(data,
         //                                             aduitLog.blobVal.size);
         //}
      }
      break;

#if defined(_WIN32) && !defined(VM_WIN_UWP)
      // For Borathon
      case MKSVchanPacketType_SmartCardInfo:
      {
         RPCVariant scInfo(this);

         if (!isDataValid(&scInfo, messageCtx)) {
            return;
         }

         Log("%s: Received Smart Card Client Info of size %d.\n", __FUNCTION__,
             scInfo.blobVal.size);
         char * data = reinterpret_cast<char *>(scInfo.blobVal.blobData);
         MKSVchanPlugin_SaveSmartCardInfo(data, scInfo.blobVal.size);
      }
      break;
#endif


      case MKSVchanPacketType_ClipboardRequest:
      {
         Log("%s: Received clipboard request.\n", __FUNCTION__);

         // Check if any policy disables sending the clipboard
         if (MKSVchan_ClipboardToClientEnabled()) {
            MKSVchanPlugin_SendClipboardData();
         } else {
            Log("%s: Sending the clipboard is disabled by policy. Ignoring clipboard request.\n",
               __FUNCTION__);
         }
      }
      break;

      case MKSVchanPacketType_ClipboardData_Text:
      case MKSVchanPacketType_ClipboardData_CPClipboard:
      {
         if (MKSVchan_ClipboardToServerEnabled()) {
            int paramCount = iChannelCtx->v1.GetParamCount(messageCtx);

            /*
             * We can receive below possible data
             * 1. Clipboard data - success case
             * 2. Clipboard data and clipboard error.
             *    Currently we support only maximum limit exceeded error in which case
             *    the clipboard data will truncated to the maximum allowed limit. This is
             *    done only in the case of text since images and rtf cannot be truncated.
             * 3. Clipboard error.
             *    In cases of rtf and images exceeding maximum limit, we send only the error.
             *
             */
            switch (paramCount) {
               case 1:
               {
                  RPCVariant varData(this);
                  char varName[CLIPBOARD_PARM_MAXLEN];
                  if (!iChannelCtx->v1.GetNamedParam(messageCtx, 0, varName, CLIPBOARD_PARM_MAXLEN, &varData)) {
                     Log("%s: Could not retrieve variant at parameter 0\n", __FUNCTION__);
                     return;
                  }

                  /* if we find blob type at parm 0, it is clipboard data - older clients do not send
                   * named params.
                   */
                  if (strcmp(varName, CLIPBOARD_DATA_PARM_NAME) == 0 || varData.vt == VDP_RPC_VT_BLOB) {
                     // Found clipboard data
                     Log("%s: Received message of size %d.\n", __FUNCTION__, varData.blobVal.size);
                     MKSVchan_SetClipboard(receivedPacketType,
                                           reinterpret_cast<uint8*>(varData.blobVal.blobData),
                                           varData.blobVal.size);
                  } else if (strcmp(varName, CLIPBOARD_ERROR_PARM_NAME) == 0) {
                     // Found clipboard error
                     Log("%s: Received error message = %s.\n", __FUNCTION__,
                        GetMKSVchanClipboardErrorAsString((MKSVCHAN_CLIPBOARD_ERROR)varData.ulVal));
                  }
                  else {
                     Log("%s: Error - no clipboard data or error was found at param 0.\n", __FUNCTION__);
                  }
               }
               break;

               case 2:
               {
                  RPCVariant varClipboardData(this), varClipboardError(this);
                  char var1Name[CLIPBOARD_PARM_MAXLEN], var2Name[CLIPBOARD_PARM_MAXLEN];
                  if (!iChannelCtx->v1.GetNamedParam(messageCtx, 0, var1Name, CLIPBOARD_PARM_MAXLEN, &varClipboardData)) {
                     Log("%s: Could not retrieve variant at parameter 0\n", __FUNCTION__);
                     return;
                  }
                  if (!iChannelCtx->v1.GetNamedParam(messageCtx, 1, var2Name, CLIPBOARD_PARM_MAXLEN, &varClipboardError)) {
                     Log("%s: Could not retrieve variant at parameter 1\n", __FUNCTION__);
                     return;
                  }

                  /* 2 params are sent by newer clients and they also send named params.
                   * So we can just compare the names
                   */
                  if (strcmp(var1Name, CLIPBOARD_DATA_PARM_NAME)) {
                     Log("%s: Error - no clipboard data found at parm 0.\n", __FUNCTION__);
                     return;
                  }
                  if (strcmp(var2Name, CLIPBOARD_ERROR_PARM_NAME)) {
                     Log("%s: Error - no clipboard error found at parm 1.\n", __FUNCTION__);
                     return;
                  }
                  Log("%s: Received message of size %d.\n", __FUNCTION__, varClipboardData.blobVal.size);
                  Log("%s: Received error message = %s.\n", __FUNCTION__,
                     GetMKSVchanClipboardErrorAsString((MKSVCHAN_CLIPBOARD_ERROR)varClipboardError.ulVal));
                  MKSVchan_SetClipboard(receivedPacketType,
                                        reinterpret_cast<uint8*>(varClipboardData.blobVal.blobData),
                                        varClipboardData.blobVal.size);
               }
               break;

               default:
                  Log("%s: Error - Unexpected param count = %d\n", __FUNCTION__, paramCount);
            }
         } else {
            Log("%s: Setting the clipboard is disabled by policy. Ignoring clipboard data.\n",
               __FUNCTION__);
         }
      }
      break;

#if defined(_WIN32) && !defined(VM_WIN_UWP)
      case MKSVchanPacketType_FileTransferRequest:
      {
         RPCVariant requestData(this);

         if (!isDataValid(&requestData, messageCtx)) {
            return;
         }

         uint8* requestDataPtr = reinterpret_cast<uint8*>(requestData.blobVal.blobData);
         FT::ReceiveRequest(requestDataPtr, requestData.blobVal.size);
      }
      break;

      case MKSVchanPacketType_FileTransferData_File:
      {
         if (MKSVchan_FileTransfer_ToServerEnabled()) {
            RPCVariant fileData(this);

            if (!isDataValid(&fileData, messageCtx)) {
               return;
            }

            uint8* fileDataPtr = reinterpret_cast<uint8*>(fileData.blobVal.blobData);
            FT::ReceiveFileData(fileDataPtr, fileData.blobVal.size);
         }
      }
      break;

      case MKSVchanPacketType_FileTransfer_Config:
      {
         RPCVariant config(this);

         if (!isDataValid(&config, messageCtx)) {
            return;
         }

         uint8* configDataPtr = reinterpret_cast<uint8*>(config.blobVal.blobData);
         FT::ReceiveConfig(configDataPtr, config.blobVal.size);
      }
      break;

      case MKSVchanPacketType_FileTransfer_Error:
      {
         /**
          * No implementation for MKSVchan server.
          * HTML Access handles it on client side currently.
          */
         Log("%s: MKSVchan server doesn't handle file transfer error now.\n",
            __FUNCTION__);
      }
      break;
#endif

      // DnD client related
      case MKSVchanPacketType_DnD_CopyProgress:
      {
         RPCVariant progressValue(this);
         Log("%s: Received DnD copying progress message.\n", __FUNCTION__);

         if (!isDataValid(&progressValue, messageCtx)) {
            return;
         }

         Log("%s: Received DnD copying progress data of size %d.\n",
             __FUNCTION__, progressValue.blobVal.size);

         uint32 *value =
            reinterpret_cast<uint32 *>(progressValue.blobVal.blobData);
         if (NULL != mDnDMsgHandler) {
            mDnDMsgHandler->OnRecvCopyProgress(*value);
         }
      }
      break;

      case MKSVchanPacketType_DnD_CopyDone:
      {
         RPCVariant copyDoneValue(this);
         Log("%s: Received DnD copy done message.\n", __FUNCTION__);

         if (!isDataValid(&copyDoneValue, messageCtx)) {
            return;
         }
         uint32 *doneValue = reinterpret_cast<uint32 *>(copyDoneValue.blobVal.blobData);
         Log("%s: Received DnD copy done data of size %d, value %d.", __FUNCTION__,
             copyDoneValue.blobVal.size, *doneValue);
         if (NULL != mDnDMsgHandler) {
            mDnDMsgHandler->OnRecvCopyDone(*doneValue);
         }
      }
      break;

      // FCP client related
      case MKSVchanPacketType_FCP_CopyDone:
      {
         RPCVariant copyDoneValue(this);
         if (!isDataValid(&copyDoneValue, messageCtx)) {
            return;
         }

         Log("%s: Received FCP copy done data of size %d.\n", __FUNCTION__,
             copyDoneValue.blobVal.size);

         uint32 *doneValue = reinterpret_cast<uint32 *>(copyDoneValue.blobVal.blobData);
         if (NULL != mFcpMsgHandler) {
            mFcpMsgHandler->OnRecvCopyDone(*doneValue);
         }
      }
      break;

      case MKSVchanPacketType_FCP_CopyProgress:
      {
         RPCVariant progressValue(this);
         if (!isDataValid(&progressValue, messageCtx)) {
            return;
         }

         uint32 *progress = reinterpret_cast<uint32 *>(progressValue.blobVal.blobData);
         Log("%s: Received FCP copy progress of size %d, value = %d.\n",
             __FUNCTION__, progressValue.blobVal.size, *progress);

         if (NULL != mFcpMsgHandler) {
            mFcpMsgHandler->OnRecvCopyProgress(*progress);
         }
      }
      break;

#if (defined(_WIN32) && !defined(VM_WIN_UWP)) || TARGET_OS_OSX
      // DnD client & server common
      case MKSVchanPacketType_DnD_Capabilities:
      {
         // Extract capability value
         RPCVariant dndCaps(this);

         if (!isDataValid(&dndCaps, messageCtx)) {
            return;
         }

         uint64 dndCapability = 0;
         /*
          * For DnD version 1, Server sends 32bit caps to Client and Client
          * won't send back. Version 2, Server sends 64bit caps and Client will
          * send the negotiated caps back to Agent
          */
         if (dndCaps.blobVal.size == sizeof(uint32)) {
            dndCapability = *(reinterpret_cast<uint32 *>(dndCaps.blobVal.blobData));
         } else {
            dndCapability = *(reinterpret_cast<uint64 *>(dndCaps.blobVal.blobData));
         }
         Log("%s: Received DnD capability 0x%llx.\n", __FUNCTION__,
             (unsigned long long)dndCapability);
         if (NULL != mDnDMsgHandler) {
            mDnDMsgHandler->OnRecvDnDCapability(dndCapability);
         }
      }
      break;

      case MKSVchanPacketType_DnD_ControllerRpc:
      {
         RPCVariant controllerRpc(this);
         Log("%s: Received DnD controller Rpc message.\n", __FUNCTION__);

         if (!isDataValid(&controllerRpc, messageCtx)) {
            return;
         }

         Log("%s: Received DnD controller Rpc data of size %d.\n", __FUNCTION__,
             controllerRpc.blobVal.size);

         uint8* data = reinterpret_cast<uint8*>(controllerRpc.blobVal.blobData);
         if (NULL != mDnDMsgHandler) {
            mDnDMsgHandler->OnRecvDnDRpcPacket(data, controllerRpc.blobVal.size);
         }
      }
      break;

      // DnD server related
      case MKSVchanPacketType_DnD_TempFolderSharedPath:
      {
         RPCVariant tempSharedPath(this);
         Log("%s: Received temp shared path from client.\n", __FUNCTION__);

         if (!isDataValid(&tempSharedPath, messageCtx)) {
            return;
         }

         Log("%s: Received temp shared path size is %d.\n", __FUNCTION__,
             tempSharedPath.blobVal.size);
         if (tempSharedPath.blobVal.size == 0) {
            return;
         }

         uint8* data = reinterpret_cast<uint8*>(tempSharedPath.blobVal.blobData);
         // Copy the dragging paths from agent to client
         if (NULL != mDnDMsgHandler) {
            mDnDMsgHandler->OnRecvClientTmpFolder(data,
                                                  tempSharedPath.blobVal.size);
         }
      }
      break;

      case MKSVchanPacketType_DnD_FilePaths:
      {
         RPCVariant filePaths(this);
         Log("%s: Received dragging paths from client.\n", __FUNCTION__);

         if (!isDataValid(&filePaths, messageCtx)) {
            return;
         }

         Log("%s: Received file paths size is %d.\n", __FUNCTION__,
             filePaths.blobVal.size);
         if (filePaths.blobVal.size == 0) {
            return;
         }

         uint8* data = reinterpret_cast<uint8*>(filePaths.blobVal.blobData);
         // Copy the dragging paths from client to agent
         if (NULL != mDnDMsgHandler) {
            mDnDMsgHandler->OnRecvFilePaths(data, filePaths.blobVal.size);
         }
      }
      break;

      case MKSVchanPacketType_DnD_CancelCopy:
      {
         Log("%s: Received notification to cancel DnD Copying.\n",
             __FUNCTION__);
         if (NULL != mDnDMsgHandler) {
            mDnDMsgHandler->OnRecvCancelCopy();
         }
      }
      break;

      // FCP
      case MKSVchanPacketType_FCP_StartPasteFiles:
      {
         if (NULL != mFcpMsgHandler) {
            mFcpMsgHandler->OnRecvStartPasteFiles();
         }
      }
      break;

      case MKSVchanPacketType_FCP_SharedFolderFName:
      case MKSVchanPacketType_FCP_TempFolderFName:
      {
         RPCVariant friendlyName(this);
         if (!isDataValid(&friendlyName, messageCtx)) {
            return;
         }

         Log("%s: Received shared folder friendly name size is %d.\n",
             __FUNCTION__, friendlyName.blobVal.size);
         if (friendlyName.blobVal.size == 0) {
            return;
         }

         uint8 *data = reinterpret_cast<uint8 *>(friendlyName.blobVal.blobData);
         if (NULL != mFcpMsgHandler) {
            if (receivedPacketType == MKSVchanPacketType_FCP_SharedFolderFName) {
               mFcpMsgHandler->OnRecvFilePaths(data, friendlyName.blobVal.size);
            } else {
               mFcpMsgHandler->OnRecvClientTmpFolder(data,
                  friendlyName.blobVal.size);
            }
         }
      }
      break;

      case MKSVchanPacketType_FCP_CancelCopy:
      {
         if (NULL != mFcpMsgHandler) {
            mFcpMsgHandler->OnRecvCancelCopy();
         }
      }
      break;
#endif

      case MKSVchanPacketType_ClipboardState:
      {
         // Extract clipboard policy state value
         RPCVariant policyData(this);

         if (!isDataValid(&policyData, messageCtx)) {
            return;
         }

         uint32 *policyValuePtr = reinterpret_cast<uint32 *>(policyData.blobVal.blobData);
         LOG_INFO("Received clipboard policy state = %s\n",
                  GetMKSVchanClipboardPolicyAsString((ClipboardPolicy)(*policyValuePtr)));
      }
      break;
      default:
         Log("%s: Received unknown packet type = %s\n", __FUNCTION__,
             GetMKSVchanPacketTypeAsString(receivedPacketType));
   }
   NotifyForRegisteredOnInvokePacketType(receivedPacketType);
}


/*
 *---------------------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::SendMessage --
 *
 *    Called by the MKSVchanPlugin to send different type of clipboard packets
 *
 * Results:
 *     It sends the packet type (clipboard request or send clipboard data) as the
 *     command in the channel context. It sets the data as a variant blob.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------------------
 */

Bool
MKSVchanRPCPlugin::SendMessage(MKSVchanPacketType packetType, // IN: format of data
                               uint8 *data,                   // IN: clipboard data
                               uint32 dataLen)                // IN: length of data
{
   if (!IsReady()) {
      Log("%s: VDPService channel has been disconnected or isn't ready.\n",
          __FUNCTION__);
      return FALSE;
   }

   const VDPRPC_ChannelContextInterface* iChannelCtx = ChannelContextInterface();

   // Create the message context using RPCManager API
   void* messageCtx = NULL;
   if (!CreateMessage(&messageCtx, RPC_CHANNEL_TYPE_CONTROL)) {
      Log("%s: Something went wrong while calling CreateMessage.\n", __FUNCTION__);
      return FALSE;
   }

   // Set command as the packet type
   RPCVariant varData(this);
   if (packetType == MKSVchanPacketType_LegacyDnD_Data) {
      iChannelCtx->v1.SetCommand(messageCtx, MKSVchanPacketType_ClipboardData_CPClipboard);
   } else {
      iChannelCtx->v1.SetCommand(messageCtx, packetType);
   }

   // Add the data blob
   if (0 != dataLen) {
      // Initialize the request class with request id and clipboard data length
      uint32 reqId = iChannelCtx->v1.GetId(messageCtx);
      if (packetType == MKSVchanPacketType_FileTransferData_File) {
         m_requestList.push_back(MKSVchanCPRequest(reqId, dataLen,
                                                   MKSVchanCPRequest::MKS_FileTransfer_Data,
                                                   packetType));
      } else if (packetType == MKSVchanPacketType_LegacyDnD_Data) {
         m_requestList.push_back(MKSVchanCPRequest(reqId, dataLen,
                                                   MKSVchanCPRequest::MKS_DropInteraction_Data,
                                                   MKSVchanPacketType_ClipboardData_CPClipboard,
                                                   MKSVchan_OnDataSentDone));
      } else {
         m_requestList.push_back(MKSVchanCPRequest(reqId, dataLen,
                                                   MKSVchanCPRequest::MKS_Clipboard_Data,
                                                   packetType,
                                                   MKSVchan_OnDataSentDone));
      }

      VDP_RPC_BLOB dataBlob;
      dataBlob.size = dataLen;
      dataBlob.blobData = reinterpret_cast<char*>(data);
      VariantInterface()->v1.VariantFromBlob(&varData, &dataBlob);
      iChannelCtx->v1.AppendNamedParam(messageCtx, CLIPBOARD_DATA_PARM_NAME, &varData);
   }

   /*
    * Always send the error report for clipboard.
    * In case of text > max allowed, we truncate the clipboard data.
    * But in cases of rtf, pictures, we do not send anything to the other side.
    * In these cases, lets report the error so that the other side can take
    * appropriate action if needed.
    */
   if (g_clipboardError != MKSVCHAN_CLIPBOARD_ERROR_NONE) {
      RPCVariant varClipboardError(this);
      VariantInterface()->v1.VariantFromUInt32(&varClipboardError, g_clipboardError);
      iChannelCtx->v1.AppendNamedParam(messageCtx, CLIPBOARD_ERROR_PARM_NAME, &varClipboardError);

      // Reset the error
      g_clipboardError = MKSVCHAN_CLIPBOARD_ERROR_NONE;
   }

   if (!InvokeMessage(messageCtx, TRUE, RPC_CHANNEL_TYPE_CONTROL)) {
      Log("%s: Invoke message failed. Destroying the message.\n", __FUNCTION__);
      DestroyMessage(messageCtx);
      return FALSE;
   }

   return TRUE;
}


/*
 *---------------------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::RegisterOnDonePacketType --
 *
 *    Register the packet type that we want to be notified in OnDone() callback.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::RegisterOnDonePacketType(MKSVchanPacketType packetType) // IN
{
   onDonePacketTypeList.push_back(packetType);
   Log("%s: onDonePacketTypeList size is %lu\n",
       __FUNCTION__, (unsigned long)onDonePacketTypeList.size());
}


/*
 *---------------------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::RegisterOnInvokePacketType --
 *
 *    Register the packet type that we want to be notified in OnInvoke() callback.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::RegisterOnInvokePacketType(MKSVchanPacketType packetType) // IN
{
   onInvokePacketTypeList.push_back(packetType);
   Log("%s: onInvokePacketTypeList size is %lu\n",
       __FUNCTION__, (unsigned long)onInvokePacketTypeList.size());
}


/*
 *---------------------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::NotifyForRegisteredOnDonePacketType --
 *
 *    Call the callback for the registered packet type.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::NotifyForRegisteredOnDonePacketType(MKSVchanCPRequestIt it) // IN
{
   if (!it->m_onDoneHandler) {
      return;
   }

   RegisteredOnDonePacketTypeList::iterator ptit = onDonePacketTypeList.begin();
   while (ptit != onDonePacketTypeList.end()) {
      if (it->m_packetType == *ptit) {
         Log("%s: onDone callback fire for type %s\n",
             __FUNCTION__, GetMKSVchanPacketTypeAsString(it->m_packetType));
         it->m_onDoneHandler(it->m_packetType);
         break;
      }
      ++ptit;
   }
}


/*
 *---------------------------------------------------------------------------------------
 *
 * MKSVchanRPCPlugin::NotifyForRegisteredOnInvokePacketType --
 *
 *    Call the callback for the registered packet type.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *---------------------------------------------------------------------------------------
 */

void
MKSVchanRPCPlugin::NotifyForRegisteredOnInvokePacketType(MKSVchanPacketType type) // IN
{
   RegisteredOnInvokePacketTypeList::iterator ptit = onInvokePacketTypeList.begin();
   while (ptit != onInvokePacketTypeList.end()) {
      if (type == *ptit) {
         Log("%s: onInvoke callback fire for type %s\n",
             __FUNCTION__, GetMKSVchanPacketTypeAsString(type));
         MKSVchan_OnInvokeDone(type);
         break;
      }
      ++ptit;
   }
}


class PollPatchPacket: M640GKitBasePacket, M640GKitBasePacketProtocol {
    typealias T = SynchronizePacketResponse

    let commandType: UInt8 = CommandType.POLL_PATCH
    let mimimumDataSize: Int = 9

    func getRequestBytes() -> Data {
        Data()
    }

    func parseResponse() -> SynchronizePacketResponse {
        NotificationPacket().handle(
            state: PatchState(rawValue: totalData[6]) ?? .none,
            fieldMask: UInt16(totalData.subdata(in: 7 ..< 9).toUInt64()),
            syncData: totalData.subdata(in: 9 ..< totalData.count)
        )
    }
}

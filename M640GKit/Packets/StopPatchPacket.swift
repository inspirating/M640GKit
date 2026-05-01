struct StopPatchResponse {
    let sequence: Double
    let patchId: Double
}

class StopPatchPacket: M640GKitBasePacket, M640GKitBasePacketProtocol {
    typealias T = StopPatchResponse
    let commandType: UInt8 = CommandType.STOP_PATCH
    let mimimumDataSize: Int = 10

    func getRequestBytes() -> Data {
        Data()
    }

    func parseResponse() -> StopPatchResponse {
        StopPatchResponse(
            sequence: totalData.subdata(in: 6 ..< 8).toDouble(),
            patchId: totalData.subdata(in: 8 ..< 10).toDouble()
        )
    }
}

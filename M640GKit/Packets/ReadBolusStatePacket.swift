struct ReadBolusStateResponse {
    let hasBolus: Bool
    let delivered: Double
    let remaining: Double
    let amount: Double
}

class ReadBolusStatePacket: M640GKitBasePacket, M640GKitBasePacketProtocol {
    typealias T = ReadBolusStateResponse

    let commandType: UInt8 = CommandType.READ_BOLUS_STATE
    let mimimumDataSize: Int = 16

    func getRequestBytes() -> Data {
        Data()
    }

    func parseResponse() -> ReadBolusStateResponse {
        let hasBolus = totalData[6] == 1
        let delivered = totalData.subdata(in: 8 ..< 10).toDouble() * 0.05
        let remaining = totalData.subdata(in: 10 ..< 12).toDouble() * 0.05
        let amount = totalData.subdata(in: 12 ..< 14).toDouble() * 0.05

        return ReadBolusStateResponse(
            hasBolus: hasBolus,
            delivered: delivered,
            remaining: remaining,
            amount: amount
        )
    }
}

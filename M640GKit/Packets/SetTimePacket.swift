struct SetTimePacketResponse {}

class SetTimePacket: M640GKitBasePacket, M640GKitBasePacketProtocol {
    typealias T = SetTimePacketResponse

    let commandType: UInt8 = CommandType.SET_TIME
    let mimimumDataSize: Int = 0
    let date: Date

    init(date: Date) {
        self.date = date
    }

    func getRequestBytes() -> Data {
        var output = Data([2])
        output.append(date.toM640GKitSeconds())

        return output
    }

    func parseResponse() -> SetTimePacketResponse {
        SetTimePacketResponse()
    }
}

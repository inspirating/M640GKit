struct SetTimePacketResponse {}

class SetTimePacket: M640GBasePacket, M640GBasePacketProtocol {
    typealias T = SetTimePacketResponse

    let commandType: UInt8 = CommandType.SET_TIME
    let mimimumDataSize: Int = 0
    let date: Date

    init(date: Date) {
        self.date = date
    }

    func getRequestBytes() -> Data {
        var output = Data([2])
        output.append(date.toM640GSeconds())

        return output
    }

    func parseResponse() -> SetTimePacketResponse {
        SetTimePacketResponse()
    }
}

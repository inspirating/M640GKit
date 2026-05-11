struct GetTimePacketResponse {
    let time: Date
}

class GetTimePacket: M640GBasePacket, M640GBasePacketProtocol {
    typealias T = GetTimePacketResponse

    let commandType: UInt8 = CommandType.GET_TIME
    let mimimumDataSize: Int = 10

    func getRequestBytes() -> Data {
        Data()
    }

    func parseResponse() -> GetTimePacketResponse {
        let secondsPassed = totalData.subdata(in: 6 ..< 10).toUInt64()
        return GetTimePacketResponse(
            time: Date.fromM640GSeconds(secondsPassed)
        )
    }
}

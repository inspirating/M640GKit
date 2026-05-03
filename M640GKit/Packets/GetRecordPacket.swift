struct GetRecordResponse {
    let recordType: UInt8
    let timestamp: Date
    let value: UInt8
}

class GetRecordPacket: M640GKitBasePacket, M640GKitBasePacketProtocol {
    typealias T = GetRecordResponse

    let commandType: UInt8 = CommandType.GET_RECORD
    let mimimumDataSize: Int = 26

    private let recordType: UInt8
    private let recordIndex: UInt8

    init(recordType: UInt8, recordIndex: UInt8 = 0) {
        self.recordType = recordType
        self.recordIndex = recordIndex
    }

    func getRequestBytes() -> Data {
        Data([recordType, recordIndex])
    }

    func parseResponse() -> GetRecordResponse {
        let timestamp = Date.fromM640GKitSeconds(totalData.subdata(in: 8 ..< 12).toUInt64())
        let value = totalData[12]

        return GetRecordResponse(
            recordType: totalData[6],
            timestamp: timestamp,
            value: value
        )
    }
}

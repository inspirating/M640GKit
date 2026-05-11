struct ResumePumpPacketResponse {}

class ResumePumpPacket: M640GBasePacket, M640GBasePacketProtocol {
    typealias T = ResumePumpPacketResponse

    let commandType: UInt8 = CommandType.RESUME_PUMP
    let mimimumDataSize: Int = 0

    func getRequestBytes() -> Data {
        Data()
    }

    func parseResponse() -> ResumePumpPacketResponse {
        ResumePumpPacketResponse()
    }
}

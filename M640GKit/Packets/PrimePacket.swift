struct PrimePacketResponse {}

class PrimePacket: M640GBasePacket, M640GBasePacketProtocol {
    typealias T = PrimePacketResponse

    let commandType: UInt8 = CommandType.PRIME
    let mimimumDataSize: Int = 0

    func getRequestBytes() -> Data {
        Data([])
    }

    func parseResponse() -> PrimePacketResponse {
        PrimePacketResponse()
    }
}

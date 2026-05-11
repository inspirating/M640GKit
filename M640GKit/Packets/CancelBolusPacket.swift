struct CancelBolusPacketResponse {}

class CancelBolusPacket: M640GBasePacket, M640GBasePacketProtocol {
    typealias T = CancelBolusPacketResponse

    let commandType: UInt8 = CommandType.CANCEL_BOLUS
    let mimimumDataSize: Int = 0

    /**
        1 -> Normal bolus
        2 -> Extended bolus
        3 -> Combi bolus
     */
    private let bolusType: UInt8 = 1

    func getRequestBytes() -> Data {
        Data([bolusType])
    }

    func parseResponse() -> CancelBolusPacketResponse {
        CancelBolusPacketResponse()
    }
}

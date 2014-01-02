/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include <ksync/ksync_index.h>
#include "ksync/mirror_ksync.h"
#include "ksync/nexthop_ksync.h"
#include "ksync/ksync_init.h"
#include <ksync/ksync_sock.h>

MirrorKSyncEntry::MirrorKSyncEntry(const MirrorKSyncEntry *entry,
                                   uint32_t index, MirrorKSyncObject *obj) : 
    KSyncNetlinkDBEntry(index), vrf_id_(entry->vrf_id_), 
    sip_(entry->sip_), sport_(entry->sport_), dip_(entry->dip_), 
    dport_(entry->dport_), ksync_obj_(obj), 
    analyzer_name_(entry->analyzer_name_) {
}

MirrorKSyncEntry::MirrorKSyncEntry(const uint32_t vrf_id, uint32_t dip,
                                   uint16_t dport, MirrorKSyncObject *obj) :
    KSyncNetlinkDBEntry(kInvalidIndex), vrf_id_(vrf_id), dip_(dip), 
    dport_(dport), ksync_obj_(obj) {
}

MirrorKSyncEntry::MirrorKSyncEntry(const MirrorEntry *mirror_entry,
                                   MirrorKSyncObject *obj) :
    KSyncNetlinkDBEntry(kInvalidIndex), vrf_id_(mirror_entry->GetVrfId()),
    sip_(*mirror_entry->GetSip()), sport_(mirror_entry->GetSPort()),
    dip_(*mirror_entry->GetDip()), dport_(mirror_entry->GetDPort()), 
    ksync_obj_(obj), nh_(NULL),
    analyzer_name_(mirror_entry->GetAnalyzerName()) {
}

MirrorKSyncEntry::MirrorKSyncEntry(std::string &analyzer_name,
                                   MirrorKSyncObject *obj) :
    ksync_obj_(obj), analyzer_name_(analyzer_name) { 
}

MirrorKSyncEntry::~MirrorKSyncEntry() {
}

KSyncDBObject *MirrorKSyncEntry::GetObject() {
    return ksync_obj_;
}

bool MirrorKSyncEntry::IsLess(const KSyncEntry &rhs) const {
    const MirrorKSyncEntry &entry = static_cast<const MirrorKSyncEntry &>(rhs);
    return (analyzer_name_ < entry.analyzer_name_);
}

std::string MirrorKSyncEntry::ToString() const {
    std::stringstream s;
    NHKSyncEntry *nh_entry = nh();

    if (nh_entry) {
        s << "Mirror Index : " << GetIndex() << " NH : " << nh_entry->GetIndex()
            << " Vrf : " << vrf_id_;
    } else {
        s << "Mirror Index : " << GetIndex() << " NH : <null>" 
            << " Vrf : " << vrf_id_;
    }
    return s.str();
}

bool MirrorKSyncEntry::Sync(DBEntry *e) {
    bool ret = false;
    const MirrorEntry *mirror = static_cast<MirrorEntry *>(e);

    NHKSyncObject *nh_object = ksync_obj_->agent()->ksync()->nh_ksync_obj();
    if (mirror->GetNH() == NULL) {
        LOG(DEBUG, "nexthop in Mirror entry is null");
        assert(0);
    }
    NHKSyncEntry nh_entry(mirror->GetNH(), nh_object);
    NHKSyncEntry *old_nh = nh();

    nh_ = nh_object->GetReference(&nh_entry);
    if (old_nh != nh()) {
        ret = true;
    }

    return ret;
}

int MirrorKSyncEntry::Encode(sandesh_op::type op, char *buf, int buf_len) {
    vr_mirror_req encoder;
    int encode_len, error;
    NHKSyncEntry *nh_entry = nh();

    encoder.set_h_op(op);
    encoder.set_mirr_index(GetIndex());
    encoder.set_mirr_rid(0);
    encoder.set_mirr_nhid(nh_entry->GetIndex());
    encode_len = encoder.WriteBinary((uint8_t *)buf, buf_len, &error);
    LOG(DEBUG, "Mirror index " << GetIndex() << " nhid " 
            << nh_entry->GetIndex());
    return encode_len;
}

int MirrorKSyncEntry::AddMsg(char *buf, int buf_len) {
    LOG(DEBUG, "MirrorEntry: Add");
    return Encode(sandesh_op::ADD, buf, buf_len);
}

int MirrorKSyncEntry::ChangeMsg(char *buf, int buf_len){
    return AddMsg(buf, buf_len);
}

int MirrorKSyncEntry::DeleteMsg(char *buf, int buf_len) {
    LOG(DEBUG, "MirrorEntry: Delete");
    return Encode(sandesh_op::DELETE, buf, buf_len);
}

KSyncEntry *MirrorKSyncEntry::UnresolvedReference() {
    NHKSyncEntry *nh_entry = nh();
    if (!nh_entry->IsResolved()) {
        return nh_entry;
    }
    return NULL;
}

MirrorKSyncObject::MirrorKSyncObject(Agent *agent) : 
    KSyncDBObject(kMirrorIndexCount), agent_(agent) {
}

MirrorKSyncObject::~MirrorKSyncObject() {
}

void MirrorKSyncObject::RegisterDBClients() {
    RegisterDb(agent_->GetMirrorTable());
}

KSyncEntry *MirrorKSyncObject::Alloc(const KSyncEntry *entry, uint32_t index) {
    const MirrorKSyncEntry *mirror_entry = 
        static_cast<const MirrorKSyncEntry *>(entry);
    MirrorKSyncEntry *ksync = new MirrorKSyncEntry(mirror_entry, index, this);
    return static_cast<KSyncEntry *>(ksync);
}

KSyncEntry *MirrorKSyncObject::DBToKSyncEntry(const DBEntry *e) {
    const MirrorEntry *mirror_entry = static_cast<const MirrorEntry *>(e);
    MirrorKSyncEntry *ksync = new MirrorKSyncEntry(mirror_entry, this);
    return static_cast<KSyncEntry *>(ksync);
}

uint32_t MirrorKSyncObject::GetIdx(std::string analyzer_name) {
    MirrorKSyncEntry key(analyzer_name, this);
    return Find(&key)->GetIndex();
}

void vr_mirror_req::Process(SandeshContext *context) {
    AgentSandeshContext *ioc = static_cast<AgentSandeshContext *>(context);
    ioc->MirrorMsgHandler(this);
}

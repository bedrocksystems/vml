**NOTE: this file is currently a work-in-progress**

# (7 logical admits (5 `mod` WeakMem); 4 subgoal admits) `admit`s/`Admitted`s Related to VIRTIO
## (XXX logical admits; XXX subgoal admits) VIRTIO Queue Operational Model
## (XXX logical admit; XXX subgoal admits) `virtqueue/`
## (6 logical admits (5 `mod` WeakMem); 4 subgoal admits) `virtio_sg/`
### (1 code proof admit (0 `mod` WeakMem)) `XXX_spec_ok` lemmas
- `conclude_chain_use_spec'_ok` (./virtio_sg/proof/buffer/conclude_chain_use.v#380)
### (4 logical admit; 4 subgoal admits) ghost state
- conclude_chain_use_requester_send_requester (./virtio_sg/proof/buffer/conclude_chain_use.v#257)
#### (1 logical admit; 2 subgoal admits) observe that `Used.Entry.t` contains `used_entry` values
- ./virtio_sg/proof/buffer/conclude_chain_use.v#241
#### (1 logical admit; 1 subgoal admits) observe agreement between op-model/`gname` `Config`s
- ./virtio_sg/proof/buffer/conclude_chain_use.v#230
#### (1 logical admit; 1 subgoal admits) connect `driven_idx`/`used_idx` in `Rep`/op-model
- ./virtio_sg/proof/buffer/conclude_chain_use.v#187
### (1 logical admit; 4 subgoal admits) arithmetic side-conditions
#### (1 logical admit; 4 subgoal admits) bumping `used_idx`
- ./virtio_sg/proof/buffer/conclude_chain_use.v#211
- ./virtio_sg/proof/buffer/conclude_chain_use.v#212
- ./virtio_sg/proof/buffer/conclude_chain_use.v#217
- ./virtio_sg/proof/buffer/conclude_chain_use.v#218

## (TODO later: 1 logical admit; 1 subgoal admit) Weak Memory
### (1 logical admit; 1 subgoal admit) `Queue._vq_escrow` state management
- ./virtio_sg/proof/buffer/conclude_chain_use.v#362

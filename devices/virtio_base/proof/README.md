# VIRTIO Verification

BedRock Formal Methods (FM) artifacts for VIRTIO.

## Artifact Types

The FM artifacts within `vml/devices/virtio_base/proof` include:

* weak-memory logic and specifications for accessing [foreign memory](#foreign-memory)<sup>[1](#footnote-1)</sup>
* operational model<sup>[2](#footnote-2)</sup> of "Split Virtqueues" based on the [Virtual I/O Device (VIRTIO) Version 1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/cs01/virtio-v1.2-cs01.pdf) standard<sup>[3](#footnote-3)</sup>
* formal specifications/proofs<sup>[3](#footnote-3)</sup> for BedRock's C++ implementation of "Split Virtqueues"
* formal specifications/proofs<sup>[3](#footnote-3)</sup> for BedRock's C++ implementation of "Scatter-Gather (SG) Buffers" - which abstract over "Split Virtqueue" metadata interactions and expose linearized interfaces for payload interactions

## References

### Code Listing/FM Artifact Mappings

The following C++ code from [vml/devices/virtio_base](https://github.com/bedrocksystems/vml/tree/main/devices/virtio_base) relates to the FM artifacts in each corresponding sublist:

* [include/model/foreign_ptr.hpp](https://github.com/bedrocksystems/vml/blob/main/devices/virtio_base/include/model/foreign_ptr.hpp): utilities for reading/writing memory that's not owned by the C++ abstract machine (e.g. device memory; i.e. `volatile` memory)
    * `proof/foreign_ptr_spec.v`:
	    * axiomatization of weak-memory modalities<sup>[1](#footnote-1)</sup>
		* model of `ForeignData`/`ForeignPtr` closing over BHV resources necessary to characterize memory access rights
		* specification of `ForeignData` and `ForeignPtr` interfaces in terms of the previous bullets
* [include/model/virtqueue.hpp](https://github.com/bedrocksystems/vml/blob/main/devices/virtio_base/include/model/virtqueue.hpp)
    * `proof/model`: (*metadata*) [defined wildness](#defined-wildness) operational model for "Split Virtqueues"
    * `proof/virtqueue`:
        * `ghost.v`: [Iris](https://iris-project.org/) ghost state constructions embedding operational model state and transitions into separation logic
        * `defs.v`: separation logic definitions bundling physical and `ghost` ownership associated with the `class`es defined in `include/model/virtqueue.hpp`
        * `spec.v`: hoare-triple style specifications of the functions declared and defined within `include/model/virtqueue.hpp`
* [src/virtqueue.cpp](https://github.com/bedrocksystems/vml/blob/main/devices/virtio_base/src/virtqueue.cpp)
    * (*omitted*) `proof/virtqueue/proof`: verification of C++ implementation against Coq specifications
* [include/model/virtio_sg.hpp](https://github.com/bedrocksystems/vml/blob/main/devices/virtio_base/include/model/virtio_sg.hpp)
    * `proof/model`: (*payload*) [defined wildness](#defined-wildness) operational model for "Split Virtqueues"
    * `proof/virtio_sg`:
        * `ghost.v`: [Iris](https://iris-project.org/) ghost state constructions embedding operational model state and transitions into separation logic
        * `defs.v`: separation logic definitions bundling physical and `ghost` ownership associated with the `class`es defined in `include/model/virtqueue.hpp`
        * `spec.v`: hoare-triple style specifications of the functions declared and defined within `include/model/virtqueue.hpp`
* [src/virtio_sg.cpp](https://github.com/bedrocksystems/vml/blob/main/devices/virtio_base/src/virtio_sg.cpp)
    * (*omitted*) `proof/virtio_sg/proof`: verification of C++ implementation against Coq specifications

### Glossary

* <a name="foreign-memory"></a>Foreign Memory: memory which is shared with processes outside of the C++ abstract machine (i.e. devices, untrusted guest processes).

### Footnotes

<small><a name="footnote-1">1</a>: Weak memory modalities have only been sketched and are thus **not** connected formally to our logic. However, we do use these modalities in approximately the right way within our VIRTIO specifications/proofs given that we aspire to prove the correctness of this C++ code under weak-memory assumptions *eventually*.</small><br>
<small><a name="footnote-2">2</a>: The op-model is written assuming an adversarial counterpart and is currently specialized to VIRTIO Devices (of which the `vSwitch` is an example). Observations of protocol violations justify non-conformant behaviors (e.g. nondeterministically transitioning, which we currently do); in practice we will need to prune "bad" parts of the model state but retain other, valid parts. One goal is to prove that the composition of a "good" VIRTIO Device and VIRTIO Driver results in conformant traces.</small><br>
<small><a name="footnote-3">3</a>: The current BedRock FM artifacts for VIRTIO are specialized to Version 1.1, but we plan to move to Version 1.2 as part of HASH.</small><br>
<small><a name="footnote-4">4</a>: The specifications/proofs are in an incomplete state since the priority has been to support stable development of vswitch specifications/proofs; under the auspices of HASH these specifications/proofs shall be completed.</small><br>

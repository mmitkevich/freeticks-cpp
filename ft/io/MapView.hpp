/*class PeersMap {
        using ServicesIterator = typename ServicesMap::iterator;
        using PeersIterator = typename Service::PeersMap::iterator;
      public:
        using key_type = decltype(std::declval<PeersIterator>()->first);
        using mapped_type = decltype(std::declval<PeersIterator>()->second);
        
        PeersMap(ServicesIterator begin, ServicesIterator end)
        : begin_(begin)
        , end_(end) {}

        class iterator {
          public:
            iterator() = default;
            iterator(ServicesIterator svc_it, ServicesIterator svc_end)
            : svc_it_(svc_it)
            , svc_end_(svc_end)
            { 
                if(svc_it_!=svc_end_) {
                    maybe_peer_it_ = peers().begin();
                }
            }
            typename Service::PeersMap& peers() {
                return svc_it_->second->peers();
            }

            auto& operator*() { return *maybe_peer_it_.value(); }
            auto* operator->() { return &(*maybe_peer_it_.value()); }
            auto& operator++() { advance(); return *this; }
            //auto& operator++(int) { advance(); return *this;}
            friend bool operator==(const iterator& lhs, const iterator& rhs) {
                return lhs.svc_it_ == rhs.svc_it_ && lhs.maybe_peer_it_ == rhs.maybe_peer_it_;
            }
            friend bool operator!=(const iterator& lhs, const iterator& rhs) {
                return !operator==(lhs, rhs);
            }

            void advance() {
                if(maybe_peer_it_) {
                    ++maybe_peer_it_.value();
                    if(maybe_peer_it_.value()==peers().end()) {
                        maybe_peer_it_.reset();
                        do {
                            ++svc_it_;
                            if(svc_it_!=svc_end_)
                                maybe_peer_it_ = peers().begin();
                            else 
                                break;
                        } while(!maybe_peer_it_);
                    }
                }
            }
          private:
            ServicesIterator svc_it_;
            ServicesIterator svc_end_;            
            std::optional<PeersIterator> maybe_peer_it_;
        };
        auto begin() const { return iterator(begin_, end_); }
        auto end() const { return iterator(begin_, end_); }
      private:
        ServicesIterator begin_, end_;
    };
*/
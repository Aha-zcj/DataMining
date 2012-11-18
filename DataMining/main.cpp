//
//  main.cpp
//  DataMining
//
//  Created by Aha on 12-11-14.
//  Copyright (c) 2012年 Aha. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <vector>
#include <cmath>

using namespace std;

#define LINEMAXLENGTH 128
int KNEIGHBOURS = 83 ;
float CUT =  0.0 ;


struct User {
    float avg_rating;
    set<int> resource_ids;
    map<int, float> rating_map;
};

struct TestUser {
    int user_id;
	int resource_id;
    unsigned long similarity_count;
	float real_rating;
	float predict_rating;
};

struct SimilarityAndUseridType {
    User *user;
    float similarity;
};

struct SUCompare {
    bool operator()(const SimilarityAndUseridType &a, const SimilarityAndUseridType &b) const {
        return a.similarity > b.similarity;
    }
};



typedef set<SimilarityAndUseridType, SUCompare> KSet;
typedef set<User *> UserPtrSet;
typedef map<int, User> UserMap;
typedef map<int, UserPtrSet> ResourceMap;


//从用户ID映射到User
UserMap uid2user_map;
//从电影ID映射到User
ResourceMap rid2user_map;
//测试用户
vector<TestUser> test_user_list;

//计算target user u和neighbour user v的相似度
float calculateSimilarity(User u, User v);
//计算估计值
float calculatePredictValue(int u_user, int resource_id, KSet k_set);


int main(int argc, const char * argv[]){
    // insert code here...
    ifstream train_input_stream("80train.txt");
    //读入数据，赋给uid2user_map
    while (train_input_stream) {
        int resource_id, user_id, discard;
        float rating;
        train_input_stream >> user_id >> resource_id >> rating >>discard;
        
        UserMap::iterator user_it = uid2user_map.find(user_id);
        if (user_it != uid2user_map.end()) {
            //找到user，添加资源及评分
            user_it->second.resource_ids.insert(resource_id);
            user_it->second.rating_map.insert(pair<int, float>(resource_id, rating));
        }
        else{
            //没有找到，插入新建用户
            User new_user;
            new_user.avg_rating = 0;
            new_user.resource_ids.insert(resource_id);
            new_user.rating_map.insert(pair<int, float>(resource_id, rating));
            uid2user_map.insert(pair<int, User>(user_id, new_user));
        }
        
        train_input_stream.ignore(LINEMAXLENGTH, '\n');
    }
    train_input_stream.close();
    
    //从uid2user_map构建rid2user_map
    UserMap::iterator user_it = uid2user_map.begin();
    
    while (user_it != uid2user_map.end()) {
        //该用户评论的资源数计数器
        int resources_count = 0;
        //评论分数总和
        float rating_sum = 0.0;
        map<int, float> user_rating_map = user_it->second.rating_map;
        map<int, float>::iterator rating_it = user_rating_map.begin();
        
        while (rating_it != user_rating_map.end()) {
            ResourceMap::iterator rid2user_it = rid2user_map.find(rating_it->first);
            if (rid2user_it != rid2user_map.end()) {
                //找到user，添加资源及评分
                rid2user_it->second.insert(&(user_it->second));
            }
            else{
                //没有扎到，插入新建用户
                UserPtrSet new_user_set;
                new_user_set.insert(&(user_it->second));
                rid2user_map.insert(pair<int, UserPtrSet>(rating_it->first, new_user_set));
            }
            //资源数自增，分数累加
            resources_count++;
            rating_sum += rating_it->second;
            //指向下一个user_rating_item
            rating_it++;
        }
        //计算平均估分
        user_it->second.avg_rating = rating_sum / resources_count;
        //指向下一个user
        user_it++;
    }
    
    
    ifstream test_input_stream("test.txt");
    while (test_input_stream) {
        int discard;
        TestUser tmp;
        test_input_stream >> tmp.user_id >> tmp.resource_id >> tmp.real_rating >>discard;
        test_user_list.push_back(tmp);
        test_input_stream.ignore(LINEMAXLENGTH, '\n');
    }
    test_input_stream.close();
    
    //遍历测试数据，找出预计值
    for (CUT = 0.1; CUT <= 0.3; CUT += 0.01) {
    
    float MAE = 0.0f;
    long all_similarity_count = 0;
    int user_resource_exist_count = 0;
    vector<TestUser>::iterator test_user_it = test_user_list.begin();
    unsigned long list_size = test_user_list.size();
    for (int i = 0; i < list_size; i++) {
        //用于记录前K个相似用户
        KSet k_similar_user;
        k_similar_user.clear();
        //查看用户是否在训练集中出现过
        bool user_exist = false;
        UserMap::iterator user_it = uid2user_map.find(test_user_it->user_id);
        if (user_it != uid2user_map.end()) {
            user_exist = true;
        }
        //查看资源是否在训练集中出现过
        bool resource_exist = false;
        ResourceMap::iterator resource_it = rid2user_map.find(test_user_it->resource_id);
        if (resource_it != rid2user_map.end()) {
            resource_exist = true;
        }
        if (user_exist) {
            if (resource_exist) {
                //训练集中用户存在，资源存在
                UserPtrSet same_taste_user = resource_it->second;
                UserPtrSet::iterator same_taste_user_it = same_taste_user.begin();
                for (; same_taste_user_it != same_taste_user.end(); same_taste_user_it++) {
                    float similarity = calculateSimilarity(uid2user_map.find(test_user_it->user_id)->second, **same_taste_user_it);
                    if (similarity > 0) {
                        SimilarityAndUseridType tmp;
                        tmp.similarity = similarity;
                        tmp.user = *same_taste_user_it;
                        k_similar_user.insert(tmp);
                    }
                }
                test_user_it->similarity_count = k_similar_user.size();
                
                //............
                user_resource_exist_count++;
                all_similarity_count += test_user_it->similarity_count;
                //............
                
                
                if (test_user_it->similarity_count > 0) {
                    test_user_it->predict_rating = calculatePredictValue(test_user_it->user_id, test_user_it->resource_id, k_similar_user);
                } else {
                    test_user_it->predict_rating = uid2user_map.find(test_user_it->user_id)->second.avg_rating;
                }
            }
            else{
                //训练集中用户存在，资源不存在
                test_user_it->predict_rating = uid2user_map.find(test_user_it->user_id)->second.avg_rating;
            }
        }
        else{
            if (resource_exist) {
                //训练集中用户不存在，资源存在
                float sum = 0.0;
                unsigned long count = 0;
                UserPtrSet same_taste_user = resource_it->second;
                count = same_taste_user.size();
                UserPtrSet::iterator same_taste_user_it = same_taste_user.begin();
                for (; same_taste_user_it != same_taste_user.end(); same_taste_user_it++) {
                    sum += (**same_taste_user_it).rating_map.find(test_user_it->resource_id)->second;
                }
                test_user_it->predict_rating = sum/count;
            } else {
                //训练集中用户不存在，资源不存在
                test_user_it->predict_rating = 3;
            }
        }
        //防止溢出
        if (test_user_it->predict_rating > 5.0) {
            test_user_it->predict_rating = 5.0;
        }
        //取整数
        test_user_it->predict_rating = int(test_user_it->predict_rating+0.5);
        
        //
        MAE += fabs(test_user_it->predict_rating - test_user_it->real_rating);
        test_user_it++;
    }
    cout<<CUT<<'\t'<<all_similarity_count/user_resource_exist_count<<'\t'<<MAE/list_size<<endl;
        KNEIGHBOURS = all_similarity_count/user_resource_exist_count;
    }
    return 0;
}

float calculateSimilarity(User u, User v){
    float a = 0, b = 0, c = 0;
    set<int> u_resources = u.resource_ids;
    set<int> v_resources = v.resource_ids;
    set<int> same_resources;
    set<int>::iterator u_resource_it = u_resources.begin();
    set<int>::iterator v_resource_it = v_resources.begin();
    //求出交集
    //one
    while (u_resource_it != u_resources.end() && v_resource_it != v_resources.end()) {
        if (*u_resource_it < *v_resource_it) {
            u_resource_it++;
        }
        else{
            if(*u_resource_it > *v_resource_it){
                v_resource_it++;
            }
            else{
                same_resources.insert(*u_resource_it);
                u_resource_it++;
                v_resource_it++;
            }
        }
    }
    //two
    //same_resource_it_end = set_intersection(u_resources.begin(), u_resources.end(), v_resources.begin(), v_resources.end(), same_resource_it);
    
    //相似度计算过滤，若相同集合数量在两个人所拥有的资源数占用比例不超过CUT值，则返回-1
    float u_rate = float(same_resources.size())/float(u_resources.size());
    float v_rate = float(same_resources.size())/float(v_resources.size());
    if (u_rate < CUT && v_rate < CUT) {
        return -1.0;
    }
    
    
    //计算
    set<int>::iterator same_resource_it = same_resources.begin();
    set<int>::iterator same_resource_it_end = same_resources.end();
    while (same_resource_it != same_resource_it_end) {
        a += (u.rating_map.find(*same_resource_it)->second - u.avg_rating) * (v.rating_map.find(*same_resource_it)->second - v.avg_rating);
        b += (u.rating_map.find(*same_resource_it)->second - u.avg_rating) * (u.rating_map.find(*same_resource_it)->second - u.avg_rating);
        c += (v.rating_map.find(*same_resource_it)->second - v.avg_rating) * (v.rating_map.find(*same_resource_it)->second - v.avg_rating);
        same_resource_it++;
    }
    return a/(sqrt(b*c));
}

float calculatePredictValue(int u_user, int resource_id, KSet k_set){
    float a = 0, b = 0;
    float ru = uid2user_map.find(u_user)->second.avg_rating;
    KSet::iterator v_it = k_set.begin();
    int k_index = 0;
    for (; v_it != k_set.end() && k_index < KNEIGHBOURS; v_it++, k_index++) {
        a += v_it->similarity * (v_it->user->rating_map.find(resource_id)->second - v_it->user->avg_rating);
        b += abs(v_it->similarity);
    }
    return ru+a/b;
}













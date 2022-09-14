1 How often is due amount updated
-	Per block
-	Per xxx seconds
-	Per day
-	Even Longer

Note: update per block is actually easy for implementation as we have examples from sushi like pools. 

2 If we choose to update longer than per block, how to calculate due amount if operator add staking in the middle of an update period
-	The share change will only affect starting from next period
-	Try to calculate a proportion?

3 How often should fund be added to the contract
-	Per xxx seconds
-	Per day
-	Even longer

4 How is the reward rate be determined
-	Fixed rate (of course manually configurable)
-	Fixed rate per period (e.g. allow configure the rate for the next day/month so we have a stable update cycle, change to the rate only affect following cycles)

5 behavior when out of fund
-	Keep recording due amount?

6 Should we lock the staking for a while? This will help preventing the exploit of withdraw before proxy query for staking and deposit back after the query.
-	Lock one upstream update period?
-	Lock longer (like months)

7 How often should upstream been updated
-	Hours?
-	Day?
-	Longer?

8 Some potential design
-	A modified single token pool
-	Multiple pools, each represent one operator, so we have the potential of delegation from first day. (can be disabled by only allow operator to deposit for his pool)

9 Maybe use wrapped EVM? Code will be slightly simpler.


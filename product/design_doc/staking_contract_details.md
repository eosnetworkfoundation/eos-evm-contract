1 How often is due amount updated
-	Per block
-	Per xxx seconds
-	Per day
-	Even Longer

Note: update per block is actually easy for implementation as we have examples from sushi like pools. 

Answer: Use whatever leads to a simple solution, so block may be better

2 If we choose to update longer than per block, how to calculate due amount if operator add staking in the middle of an update period
-	The share change will only affect starting from next period
-	Try to calculate a proportion?

Answer: N/A

3 How often should fund be added to the contract
-	Per xxx seconds
-	Per day
-	Even longer

Answer: It's not a blocker here, we can decide later

4 How is the reward rate be determined
-	Fixed rate (of course manually configurable)
-	Fixed rate per period (e.g. allow configure the rate for the next day/month so we have a stable update cycle, change to the rate only affect following cycles)

Answer: Go for the easy one, so maybe fix rate

5 behavior when out of fund
-	Keep recording due amount?

Answer: Fail the call by telling them try later. Keep recording due amount. We should try to have enough margin there.

6 Should we lock the staking for a while? This will help preventing the exploit of withdraw before proxy query for staking and deposit back after the query.
-	Lock one upstream update period?
-	Lock longer (like months)

Answer: Yes, or only remove reward if you want to withdraw during lock period

7 How often should upstream been updated
-	Hours?
-	Day?
-	Longer?

Answer: Try to  find a reasonable short time that do not affect performance

8 Some potential design
-	A modified single token pool
-	Multiple pools, each represent one operator, so we have the potential of delegation from first day. (can be disabled by only allow operator to deposit for his pool)

Answer: One single pool should be fine.

9 Maybe use wrapped EVM? Code will be slightly simpler.

Answer:  decide later

10 We can accept change behavior if we want to update the mechanism to accept real traffic data
